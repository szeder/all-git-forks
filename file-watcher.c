#include "cache.h"
#include "sigchain.h"
#include "parse-options.h"
#include "exec_cmd.h"
#include "unix-socket.h"
#include "string-list.h"
#include "pkt-line.h"

static const char *const file_watcher_usage[] = {
	N_("git file-watcher [options] <socket directory>"),
	NULL
};

struct dir;
struct repository;

struct file {
	char *name;
	struct dir *parent;
	struct repository *repo;
	struct file *next;
};

struct dir {
	char *name;
	struct dir *parent;
	struct dir **subdirs;
	struct file **files;
	struct repository *repo; /* only for root note */
	int wd, nr_subdirs, nr_files;
};

static struct dir **wds;
static int wds_alloc;

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
	struct string_list updated;
	int updated_sorted;
	int updating;
	struct dir *root;
};

const char *invalid_signature = "0000000000000000000000000000000000000000";

static struct repository **repos;
static int nr_repos;

struct connection {
	int sock, polite;
	struct repository *repo;

	char new_index[41];
};

static struct connection **conns;
static struct pollfd *pfd;
static int conns_alloc, pfd_nr, pfd_alloc;
static int inotify_fd;

/*
 * IN_DONT_FOLLOW does not matter now as we do not monitor
 * symlinks. See ce_watchable().
 */
#define INOTIFY_MASKS (IN_DELETE_SELF | IN_MOVE_SELF | \
		       IN_CREATE | IN_ATTRIB | IN_DELETE | IN_MODIFY |	\
		       IN_MOVED_FROM | IN_MOVED_TO)
static struct dir *create_dir(struct dir *parent, const char *path,
			      const char *basename)
{
	struct dir *d;
	int wd = inotify_add_watch(inotify_fd, path, INOTIFY_MASKS);
	if (wd < 0)
		return NULL;

	d = xmalloc(sizeof(*d));
	memset(d, 0, sizeof(*d));
	d->wd = wd;
	d->parent = parent;
	d->name = xstrdup(basename);

	ALLOC_GROW(wds, wd + 1, wds_alloc);
	wds[wd] = d;
	return d;
}

static int get_dir_pos(struct dir *d, const char *name)
{
	int first, last;

	first = 0;
	last = d->nr_subdirs;
	while (last > first) {
		int next = (last + first) >> 1;
		int cmp = strcmp(name, d->subdirs[next]->name);
		if (!cmp)
			return next;
		if (cmp < 0) {
			last = next;
			continue;
		}
		first = next+1;
	}

	return -first-1;
}

static void free_file(struct dir *d, int pos, int topdown);
static void free_dir(struct dir *d, int topdown)
{
	struct dir *p = d->parent;
	int pos;
	if (!topdown && p && (pos = get_dir_pos(p, d->name)) < 0)
		die("How come this directory is not registered in its parent?");
	if (d->repo)
		d->repo->root = NULL;
	wds[d->wd] = NULL;
	inotify_rm_watch(inotify_fd, d->wd);
	if (topdown) {
		int i;
		for (i = 0; i < d->nr_subdirs; i++)
			free_dir(d->subdirs[i], topdown);
		for (i = 0; i < d->nr_files; i++)
			free_file(d, i, topdown);
	}
	free(d->name);
	free(d->subdirs);
	free(d->files);
	free(d);
	if (p && !topdown) {
		p->nr_subdirs--;
		memmove(p->subdirs + pos, p->subdirs + pos + 1,
			(p->nr_subdirs - pos) * sizeof(*p->subdirs));
		if (!p->nr_subdirs && !p->nr_files)
			free_dir(p, topdown);
	}
}

static int get_file_pos(struct dir *d, const char *name)
{
	int first, last;

	first = 0;
	last = d->nr_files;
	while (last > first) {
		int next = (last + first) >> 1;
		int cmp = strcmp(name, d->files[next]->name);
		if (!cmp)
			return next;
		if (cmp < 0) {
			last = next;
			continue;
		}
		first = next+1;
	}

	return -first-1;
}

static void free_file(struct dir *d, int pos, int topdown)
{
	struct file *f = d->files[pos];
	free(f->name);
	free(f);
	if (!topdown) {
		d->nr_files--;
		memmove(d->files + pos, d->files + pos + 1,
			(d->nr_files - pos) * sizeof(*d->files));
		if (!d->nr_subdirs && !d->nr_files)
			free_dir(d, topdown);
	}
}

static struct dir *add_dir(struct dir *d,
			   const char *path, const char *basename)
{
	struct dir *new;
	int pos = get_dir_pos(d, basename);
	if (pos >= 0)
		return d->subdirs[pos];
	pos = -pos-1;

	new = create_dir(d, path, basename);
	if (!new)
		return NULL;

	d->nr_subdirs++;
	d->subdirs = xrealloc(d->subdirs, sizeof(*d->subdirs) * d->nr_subdirs);
	if (d->nr_subdirs > pos + 1)
		memmove(d->subdirs + pos + 1, d->subdirs + pos,
			(d->nr_subdirs - pos - 1) * sizeof(*d->subdirs));
	d->subdirs[pos] = new;
	return new;
}

static struct file *add_file(struct dir *d, const char *name)
{
	struct file *new;
	int pos = get_file_pos(d, name);
	if (pos >= 0)
		return d->files[pos];
	pos = -pos-1;

	new = xmalloc(sizeof(*new));
	memset(new, 0, sizeof(*new));
	new->parent = d;
	new->name = xstrdup(name);

	d->nr_files++;
	d->files = xrealloc(d->files, sizeof(*d->files) * d->nr_files);
	if (d->nr_files > pos + 1)
		memmove(d->files + pos + 1, d->files + pos,
			(d->nr_files - pos - 1) * sizeof(*d->files));
	d->files[pos] = new;
	return new;
}

static int watch_path(struct repository *repo, char *path)
{
	struct dir *d = repo->root;
	char *p = path;

	if (!d) {
		d = create_dir(NULL, ".", "");
		if (!d)
			return -1;
		repo->root = d;
		d->repo = repo;
	}

	for (;;) {
		char *next, *sep;
		sep = strchr(p, '/');
		if (!sep) {
			struct file *file;
			file = add_file(d, p);
			if (!file->repo)
				file->repo = repo;
			break;
		}

		next = sep + 1;
		*sep = '\0';
		d = add_dir(d, path, p);
		if (!d)
			/* we could free oldest watches and try again */
			return -1;
		*sep = '/';
		p = next;
	}
	return 0;
}

static void get_changed_list(int conn_id)
{
	struct strbuf sb = STRBUF_INIT;
	int i, size, fd = conns[conn_id]->sock;
	struct repository *repo = conns[conn_id]->repo;

	for (i = size = 0; i < repo->updated.nr; i++)
		size += strlen(repo->updated.items[i].string) + 1;
	packet_write(fd, "changed %d", size);
	if (!size)
		return;
	strbuf_grow(&sb, size);
	for (i = 0; i < repo->updated.nr; i++)
		strbuf_add(&sb, repo->updated.items[i].string,
			   strlen(repo->updated.items[i].string) + 1);
	write_in_full(fd, sb.buf, sb.len);
}

static inline uint64_t stamp(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

static int shutdown_connection(int id);
static void watch_paths(int conn_id, char *buf, int maxlen)
{
	int ret, len, n;
	uint64_t start, now;
	char *end;

	n = strtol(buf, &end, 10);
	if (end != buf + maxlen) {
		packet_write(conns[conn_id]->sock,
			     "error invalid watch number %s", buf);
		shutdown_connection(conn_id);
		return;
	}

	buf = xmallocz(n);
	end = buf + n;
	/*
	 * Careful if this takes longer than 50ms, it'll upset other
	 * connections
	 */
	if (read_in_full(conns[conn_id]->sock, buf, n) != n) {
		shutdown_connection(conn_id);
		return;
	}
	if (chdir(conns[conn_id]->repo->work_tree)) {
		packet_write(conns[conn_id]->sock,
			     "error chdir %s", strerror(errno));
		return;
	}
	start = stamp();
	for (n = ret = 0; buf < end; buf += len + 1) {
		len = strlen(buf);
		if (watch_path(conns[conn_id]->repo, buf))
			break;
		n++;
		if (n & 0x3ff)
			continue;
		now = stamp();
		/*
		 * If we process for too long, the client may timeout
		 * and give up. Let the client know we're not dead
		 * yet, every 30ms.
		 */
		if (start + 30000 < now) {
			packet_write(conns[conn_id]->sock, "watching %d", n);
			start = now;
		}
	}
	packet_write(conns[conn_id]->sock, "watched %u", n);
}

static void unchange(int conn_id, unsigned long size)
{
	struct connection *conn = conns[conn_id];
	struct repository *repo = conn->repo;
	if (size) {
		struct strbuf sb = STRBUF_INIT;
		char *p;
		int len;
		strbuf_grow(&sb, size);
		if (read_in_full(conn->sock, sb.buf, size) <= 0)
			return;
		if (!repo->updated_sorted) {
			sort_string_list(&repo->updated);
			repo->updated_sorted = 1;
		}
		for (p = sb.buf; p - sb.buf < size; p += len + 1) {
			struct string_list_item *item;
			len = strlen(p);
			item = string_list_lookup(&repo->updated, p);
			if (!item)
				continue;
			unsorted_string_list_delete_item(&repo->updated,
							 item - repo->updated.items, 0);
		}
		strbuf_release(&sb);
	}
	memcpy(repo->index_signature, conn->new_index, 40);
	/*
	 * If other connections on this repo are in some sort of
	 * session that depend on the previous repository state, we
	 * may need to disconnect them to be safe.
	 */

	/* pfd[0] is the listening socket, can't be a connection */
	repo->updating = 0;
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
	repo->updated.strdup_strings = 1;
	repos[first] = repo;
	return repo;
}

static void reset_watches(struct repository *repo)
{
	if (repo->root)
		free_dir(repo->root, 1);
}

static void reset_repo(struct repository *repo, ino_t inode)
{
	reset_watches(repo);
	string_list_clear(&repo->updated, 0);
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
	if (conn->repo && conn->repo->updating == id)
		conn->repo->updating = 0;
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
	 * > "watch" SP LENGTH
	 * > PATH-LIST
	 * < "watching" SP NUM
	 * < "watched" SP NUM
	 *
	 * PATH-LIST is the list of paths, each terminated with
	 * NUL. PATH-LIST is not wrapped in pkt-line format. LENGTH is
	 * the size of PATH-LIST in bytes.
	 *
	 * The client asks file watcher to watcher a number of
	 * paths. File watcher starts to process from path by path in
	 * received order. File watcher returns the actual number of
	 * watched paths with "watched" command.
	 *
	 * File watcher may send any number of "watching" messages
	 * before "watched". This packet is to keep the connection
	 * alive and has no other values.
	 */
	else if (starts_with(msg, "watch ")) {
		if (!conns[conn_id]->repo) {
			packet_write(fd, "error have not received index command");
			return shutdown_connection(conn_id);
		}
		watch_paths(conn_id, msg + 6, len - 6);
	}

	/*
	 * > "get-changed"
	 * < changed SP LENGTH
	 * < PATH-LIST
	 *
	 * When watched path gets updated, the path is moved from
	 * "watched" list to "changed" list and is no longer watched.
	 * This command get the list of changed paths. PATH-LIST is
	 * also sent if LENGTH is non-zero.
	 */
	else if (!strcmp(msg, "get-changed")) {
		if (!conns[conn_id]->repo) {
			packet_write(fd, "error have not received index command");
			return shutdown_connection(conn_id);
		}
		get_changed_list(conn_id);
	}

	/*
	 * > "new-index" INDEX-SIGNATURE
	 * > "unchange" SP LENGTH
	 * > PATH-LIST
	 *
	 * "new-index" passes new index signature from the
	 * client. "unchange" sends the list of paths to be removed
	 * from "changed" list.
	 *
	 * "new-index" must be sent before "unchange". File watcher
	 * waits until the last "unchange" line, then update its index
	 * signature as well as "changed" list.
	 */
	else if (starts_with(msg, "new-index ")) {
		if (len != 50) {
			packet_write(fd, "error invalid new-index line %s", msg);
			return shutdown_connection(conn_id);
		}
		if (!conns[conn_id]->repo) {
			packet_write(fd, "error have not received index command");
			return shutdown_connection(conn_id);
		}
		if (conns[conn_id]->repo->updating == conn_id) {
			packet_write(fd, "error received new-index command more than once");
			return shutdown_connection(conn_id);
		}
		memcpy(conns[conn_id]->new_index, msg + 10, 40);
		/*
		 * if updating is non-zero the other client will get
		 * disconnected at the next "unchange" command because
		 * "updating" no longer points to its connection.
		 */
		conns[conn_id]->repo->updating = conn_id;
	}
	else if (skip_prefix(msg, "unchange ")) {
		unsigned long n;
		char *end;
		n = strtoul(msg + 9, &end, 10);
		if (end != msg + len) {
			packet_write(fd, "error invalid unchange line %s", msg);
			return shutdown_connection(conn_id);
		}
		if (!conns[conn_id]->repo) {
			packet_write(fd, "error have not received index command");
			return shutdown_connection(conn_id);
		}
		if (conns[conn_id]->repo->updating != conn_id) {
			packet_write(fd, "error have not received new-index command");
			return shutdown_connection(conn_id);
		}
		unchange(conn_id, n);
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

	inotify_fd = inotify_init();
	if (inotify_fd < 0)
		die_errno("unable to initialize inotify");

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
				if (conns[i]->repo && conns[i]->repo->updating == i)
					conns[i]->repo->updating = new_nr;
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
