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

int main(int argc, const char **argv)
{
	struct strbuf sb = STRBUF_INIT;
	int i, new_nr, fd, quit = 0, nr_common;
	const char *socket_path = NULL;
	struct option options[] = {
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
	fd = unix_stream_listen(sb.buf, 0);
	if (fd == -1)
		die_errno(_("unable to listen at %s"), sb.buf);
	strbuf_reset(&sb);

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
