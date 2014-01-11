#include "cache.h"
#include "sigchain.h"
#include "parse-options.h"
#include "exec_cmd.h"
#include "unix-socket.h"
#include "pkt-line.h"

static const char *const file_watcher_usage[] = {
	N_("git file-watcher [options]"),
	NULL
};

struct repository {
	char *work_tree;
	char index_signature[41];
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

static int watch_path(struct repository *repo, char *path)
{
	return -1;
}

static void watch_paths(int conn_id, char *buf, int maxlen)
{
	char *end = buf + maxlen;
	int n, ret, len;
	if (chdir(conns[conn_id]->repo->work_tree)) {
		packet_write(conns[conn_id]->sock,
			     "error chdir %s", strerror(errno));
		return;
	}
	for (n = ret = 0; buf < end && !ret; buf += len) {
		char ch;
		len = packet_length(buf);
		ch = buf[len];
		buf[len] = '\0';
		if (!(ret = watch_path(conns[conn_id]->repo, buf + 4)))
			n++;
		buf[len] = ch;
	}
	packet_write(conns[conn_id]->sock, "watched %u", n);
}

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
	pfd[id].fd = -1; /* pfd_nr is shrunk in the main event loop */
	close(conn->sock);
	conn->sock = -1;
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

	/*
	 * > "watch" SP PATH [PATH ...]
	 * < "watched" SP NUM
	 *
	 * PATH is wrapped in pkt-line format and is relative to
	 * WORK-TREE-PATH
	 *
	 * The client asks file watcher to watcher a number of
	 * paths. File watcher starts to process from path by path in
	 * received order. File watcher returns the actual number of
	 * watched paths.
	 */
	else if (starts_with(msg, "watch ")) {
		if (!conns[conn_id]->repo) {
			packet_write(fd, "error have not received index command");
			return shutdown_connection(conn_id);
		}
		watch_paths(conn_id, msg + 6, len - 6);
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

static void close_connection(int id)
{
	struct connection *conn = conns[id];
	if (!conn)
		return;
	conns[id] = NULL;
	close(conn->sock);
	free(conn);
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
	if (argc != 1)
		die(_("too many arguments"));

	atexit(cleanup);
	sigchain_push_common(cleanup_on_signal);

	socket_path = argv[0];
	strbuf_addf(&sb, "%s/socket", socket_path);
	if (!access(sb.buf, F_OK))
		die(_("%s already exists"), sb.buf);
	fd = unix_stream_listen(sb.buf);
	if (fd == -1)
		die_errno(_("unable to listen at %s"), sb.buf);
	strbuf_reset(&sb);

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
			if (i != new_nr) { /* move up pfd[] and conns[] */
				if (conns[new_nr])
					close_connection(new_nr);
				conns[new_nr] = conns[i];
				pfd[new_nr] = pfd[i];
			}
			if (pfd[i].revents & (POLLHUP | POLLERR | POLLNVAL))
				close_connection(i);
			else {
				if ((pfd[i].revents & POLLIN) && conns[i])
					handle_command(i);
				if (pfd[i].fd != -1)
					new_nr++;
			}
		}
		pfd_nr = new_nr;

		if (pfd[0].revents & POLLIN)
			accept_connection(pfd[0].fd);
		if (pfd[0].revents & (POLLHUP | POLLERR | POLLNVAL))
			die(_("error on listening socket"));
	}
	return 0;
}
