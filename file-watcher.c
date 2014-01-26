#include "cache.h"
#include "sigchain.h"
#include "parse-options.h"
#include "exec_cmd.h"
#include "unix-socket.h"
#include "pkt-line.h"

static const char *const file_watcher_usage[] = {
	N_("git file-watcher [options] <socket directory>"),
	NULL
};

struct repository {
	char *work_tree;
	char index_signature[41];
	/*
	 * At least with inotify we don't keep track down to "/". So
	 * if worktree is /abc/def and someone moves /abc to /ghi, and
	 * /jlk to /abc (and /jlk/def exists before the move), we
	 * cant' detect that /abc/def is totally new. Checking inode
	 * is probably enough for this case.
	 */
	ino_t inode;
};

const char *invalid_signature = "0000000000000000000000000000000000000000";

static struct repository **repos;
static int nr_repos;

struct connection {
	int sock, polite;
	struct repository *repo;
};

static struct connection **conns;
static struct pollfd *pfd;
static int conns_alloc, pfd_nr, pfd_alloc;

static struct repository *get_repo(const char *work_tree)
{
	int first, last;
	struct repository *repo;

	first = 0;
	last = nr_repos;
	while (last > first) {
		int next = (last + first) >> 1;
		int cmp = strcmp(work_tree, repos[next]->work_tree);
		if (!cmp)
			return repos[next];
		if (cmp < 0) {
			last = next;
			continue;
		}
		first = next+1;
	}

	nr_repos++;
	repos = xrealloc(repos, sizeof(*repos) * nr_repos);
	if (nr_repos > first + 1)
		memmove(repos + first + 1, repos + first,
			(nr_repos - first - 1) * sizeof(*repos));
	repo = xmalloc(sizeof(*repo));
	memset(repo, 0, sizeof(*repo));
	repo->work_tree = xstrdup(work_tree);
	memset(repo->index_signature, '0', 40);
	repos[first] = repo;
	return repo;
}

static void reset_repo(struct repository *repo, ino_t inode)
{
	memcpy(repo->index_signature, invalid_signature, 40);
	repo->inode = inode;
}

static int shutdown_connection(int id)
{
	struct connection *conn = conns[id];
	conns[id] = NULL;
	pfd[id].fd = -1;
	if (!conn)
		return 0;
	close(conn->sock);
	free(conn);
	return 0;
}

static int handle_command(int conn_id)
{
	int fd = conns[conn_id]->sock;
	int len;
	const char *arg;
	char *msg;

	/*
	 * ">" denotes an incoming packet, "<" outgoing. The lack of
	 * "<" means no reply expected.
	 *
	 * < "error" SP ERROR-STRING
	 *
	 * This can be sent whenever the client violates the protocol.
	 */

	msg = packet_read_line(fd, &len);
	if (!msg) {
		packet_write(fd, "error invalid pkt-line");
		return shutdown_connection(conn_id);
	}

	/*
	 * > "hello" [SP CAP [SP CAP..]]
	 * < "hello" [SP CAP [SP CAP..]]
	 *
	 * Advertise capabilities of both sides. File watcher may
	 * disconnect if the client does not advertise the required
	 * capabilities. Capabilities in uppercase MUST be
	 * supported. If any side does not understand any of the
	 * advertised uppercase capabilities, it must disconnect.
	 */
	if ((arg = skip_prefix(msg, "hello"))) {
		if (*arg) {	/* no capabilities supported yet */
			packet_write(fd, "error capabilities not supported");
			return shutdown_connection(conn_id);
		}
		packet_write(fd, "hello");
		conns[conn_id]->polite = 1;
	}

	/*
	 * > "index" SP INDEX-SIGNATURE SP WORK-TREE-PATH
	 * < "ok" | "inconsistent"
	 *
	 * INDEX-SIGNATURE consists of 40 hexadecimal letters
	 * WORK-TREE-PATH must be absolute and normalized path
	 *
	 * Watch file changes in index. The client sends the index and
	 * work tree info. File watcher validates that it holds the
	 * same info. If so it sends "ok" back indicating both sides
	 * are on the same page and CE_WATCHED bits can be ketpt.
	 *
	 * Otherwise it sends "inconsistent" and both sides must reset
	 * back to initial state. File watcher keeps its index
	 * signature all-zero until the client has updated the index
	 * ondisk and request to update index signature.
	 *
	 * "hello" must be exchanged first. After this command the
	 * connection is associated with a worktree/index. Many
	 * commands may require this to proceed.
	 */
	else if (starts_with(msg, "index ")) {
		struct repository *repo;
		struct stat st;
		if (!conns[conn_id]->polite) {
			packet_write(fd, "error why did you not greet me? go away");
			return shutdown_connection(conn_id);
		}
		if (len < 47 || msg[46] != ' ' || !is_absolute_path(msg + 47)) {
			packet_write(fd, "error invalid index line %s", msg);
			return shutdown_connection(conn_id);
		}

		if (lstat(msg + 47, &st) || !S_ISDIR(st.st_mode)) {
			packet_write(fd, "error work tree does not exist: %s",
				     strerror(errno));
			return shutdown_connection(conn_id);
		}
		repo = get_repo(msg + 47);
		conns[conn_id]->repo = repo;
		if (memcmp(msg + 6, repo->index_signature, 40) ||
		    !memcmp(msg + 6, invalid_signature, 40) ||
		    repo->inode != st.st_ino) {
			packet_write(fd, "inconsistent");
			reset_repo(repo, st.st_ino);
			return 0;
		}
		packet_write(fd, "ok");
	}
	else {
		packet_write(fd, "error unrecognized command %s", msg);
		return shutdown_connection(conn_id);
	}
	return 0;
}

static void accept_connection(int fd)
{
	struct connection *conn;
	int client = accept(fd, NULL, NULL);
	if (client < 0) {
		warning(_("accept failed: %s"), strerror(errno));
		return;
	}

	ALLOC_GROW(pfd, pfd_nr + 1, pfd_alloc);
	pfd[pfd_nr].fd = client;
	pfd[pfd_nr].events = POLLIN;
	pfd[pfd_nr].revents = 0;

	ALLOC_GROW(conns, pfd_nr + 1, conns_alloc);
	conn = xmalloc(sizeof(*conn));
	memset(conn, 0, sizeof(*conn));
	conn->sock = client;
	conns[pfd_nr] = conn;
	pfd_nr++;
}

static const char *socket_path;
static int do_not_clean_up;

static void cleanup(void)
{
	struct strbuf sb = STRBUF_INIT;
	if (do_not_clean_up)
		return;
	strbuf_addf(&sb, "%s/socket", socket_path);
	unlink(sb.buf);
	strbuf_release(&sb);
}

static void cleanup_on_signal(int signo)
{
	cleanup();
	sigchain_pop(signo);
	raise(signo);
}

static const char permissions_advice[] =
N_("The permissions on your socket directory are too loose; other\n"
   "processes may be able to read your file listing. Consider running:\n"
   "\n"
   "	chmod 0700 %s");
static void check_socket_directory(const char *path)
{
	struct stat st;
	char *path_copy = xstrdup(path);
	char *dir = dirname(path_copy);

	if (!stat(dir, &st)) {
		if (st.st_mode & 077)
			die(_(permissions_advice), dir);
		free(path_copy);
		return;
	}

	/*
	 * We must be sure to create the directory with the correct mode,
	 * not just chmod it after the fact; otherwise, there is a race
	 * condition in which somebody can chdir to it, sleep, then try to open
	 * our protected socket.
	 */
	if (safe_create_leading_directories_const(dir) < 0)
		die_errno(_("unable to create directories for '%s'"), dir);
	if (mkdir(dir, 0700) < 0)
		die_errno(_("unable to mkdir '%s'"), dir);
	free(path_copy);
}

int main(int argc, const char **argv)
{
	struct strbuf sb = STRBUF_INIT;
	int i, new_nr, fd, quit = 0, nr_common;
	int daemon = 0;
	struct option options[] = {
		OPT_BOOL(0, "detach", &daemon,
			 N_("run in background")),
		OPT_END()
	};

	git_extract_argv0_path(argv[0]);
	git_setup_gettext();
	argc = parse_options(argc, argv, NULL, options,
			     file_watcher_usage, 0);
	if (argc < 1)
		die(_("socket path missing"));
	else if (argc > 1)
		die(_("too many arguments"));

	socket_path = argv[0];
	strbuf_addf(&sb, "%s/socket", socket_path);
	check_socket_directory(sb.buf);
	fd = unix_stream_listen(sb.buf, 0);
	if (fd == -1)
		die_errno(_("unable to listen at %s"), sb.buf);
	strbuf_reset(&sb);

	atexit(cleanup);
	sigchain_push_common(cleanup_on_signal);

	if (daemon) {
		int err;
		strbuf_addf(&sb, "%s/log", socket_path);
		err = open(sb.buf, O_CREAT | O_TRUNC | O_WRONLY, 0600);
		adjust_shared_perm(sb.buf);
		if (err == -1)
			die_errno(_("unable to create %s"), sb.buf);
		if (daemonize(&do_not_clean_up))
			die(_("--detach not supported on this platform"));
		dup2(err, 1);
		dup2(err, 2);
		close(err);
	}

	nr_common = 1;
	pfd_alloc = pfd_nr = nr_common;
	pfd = xmalloc(sizeof(*pfd) * pfd_alloc);
	pfd[0].fd = fd;
	pfd[0].events = POLLIN;

	while (!quit) {
		if (poll(pfd, pfd_nr, -1) < 0) {
			if (errno != EINTR) {
				error("Poll failed, resuming: %s",
				      strerror(errno));
				sleep(1);
			}
			continue;
		}

		for (new_nr = i = nr_common; i < pfd_nr; i++) {
			if (pfd[i].revents & (POLLERR | POLLNVAL))
				shutdown_connection(i);
			else if ((pfd[i].revents & POLLIN) && conns[i]) {
				unsigned int avail = 1;
				/*
				 * pkt-line is not gentle with eof, at
				 * least not with
				 * packet_read_line(). Avoid feeding
				 * eof to it.
				 */
				if ((pfd[i].revents & POLLHUP) &&
				    ioctl(pfd[i].fd, FIONREAD, &avail))
					die_errno("unable to FIONREAD inotify handle");
				/*
				 * We better have a way to handle all
				 * packets in one go...
				 */
				if (avail)
					handle_command(i);
				else
					shutdown_connection(i);
			} else if (pfd[i].revents & POLLHUP)
				shutdown_connection(i);
			if (!conns[i])
				continue;
			if (i != new_nr) { /* pfd[] is shrunk, move pfd[i] up */
				conns[new_nr] = conns[i];
				pfd[new_nr] = pfd[i];
			}
			new_nr++; /* keep the good socket */
		}
		pfd_nr = new_nr;

		if (pfd[0].revents & POLLIN)
			accept_connection(pfd[0].fd);
		if (pfd[0].revents & (POLLHUP | POLLERR | POLLNVAL))
			die(_("error on listening socket"));
	}
	return 0;
}
