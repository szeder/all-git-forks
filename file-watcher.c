#include "cache.h"
#include "sigchain.h"
#include "parse-options.h"
#include "exec_cmd.h"
#include "unix-socket.h"

static const char *const file_watcher_usage[] = {
	N_("git file-watcher [options] <socket directory>"),
	NULL
};

struct connection {
	int sock;
};

static struct connection **conns;
static struct pollfd *pfd;
static int conns_alloc, pfd_nr, pfd_alloc;

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
