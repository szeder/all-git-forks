#include "cache.h"
#include "file-watcher-lib.h"
#include "pkt-line.h"
#include "unix-socket.h"

static char *watcher_path;
static int WAIT_TIME = 50;	/* in ms */

static char *read_watcher(int fd, int *dst_len)
{
	int len;

	if (fd == -1)
		return NULL;

	len = packet_read_timeout(fd, NULL, NULL,
				  packet_buffer, sizeof(packet_buffer),
				  PACKET_READ_GENTLE, WAIT_TIME);
	if (len <= 0)
		return NULL;
	if (starts_with(packet_buffer, "error ")) {
		fprintf(stderr, "watcher: %s\n", packet_buffer + 6);
		return NULL;
	}
	if (dst_len)
		*dst_len = len;
	return packet_buffer;
}

void packet_trace(const char *buf, unsigned int len, int write);
static int send_watcher(int fd, const char *fmt, ...)
{
	static struct strbuf sb = STRBUF_INIT;
	struct pollfd pfd;
	va_list args;

	if (fd == -1)
		return -1;

	va_start(args, fmt);
	strbuf_reset(&sb);
	format_packet(&sb, fmt, args);
	va_end(args);

	pfd.fd = fd;
	pfd.events = POLLOUT;
	if (poll(&pfd, 1, WAIT_TIME) > 0 &&
	    (pfd.revents & POLLOUT) &&
	    xwrite(fd, sb.buf, sb.len) == sb.len) {
		packet_trace(sb.buf + 4, sb.len - 4, 1);
		return sb.len;
	}
	return -1;
}

static int connect_watcher(const char *path)
{
	struct strbuf sb = STRBUF_INIT;
	int fd;

	if (!path)
		return -1;

	strbuf_addf(&sb, "%s/socket", path);
	fd = unix_stream_connect(sb.buf);
	strbuf_release(&sb);
	return fd;
}

static void reset_watches(struct index_state *istate, int disconnect)
{
	int i;
	for (i = 0; i < istate->cache_nr; i++)
		if (istate->cache[i]->ce_flags & CE_WATCHED) {
			istate->cache[i]->ce_flags &= ~CE_WATCHED;
			istate->cache_changed = 1;
		}
	if (disconnect && istate->watcher != -1) {
		close(istate->watcher);
		istate->watcher = -1;
	}
}

static int watcher_config(const char *var, const char *value, void *data)
{
	if (!strcmp(var, "filewatcher.path")) {
		if (!is_absolute_path(value))
			watcher_path = git_pathdup("%s", value);
		else
			watcher_path = xstrdup(value);
		return 0;
	}
	if (!strcmp(var, "filewatcher.timeout")) {
		WAIT_TIME = git_config_int(var, value);
		return 0;
	}
	return 0;
}

void open_watcher(struct index_state *istate)
{
	static int read_config = 0;
	char *msg;

	if (!read_config) {
		/*
		 * can't hook into git_default_config because
		 * read_cache() may be called even before git_config()
		 * call.
		 */
		git_config(watcher_config, NULL);
		read_config = 1;
	}

	istate->watcher = connect_watcher(watcher_path);
	if (send_watcher(istate->watcher, "hello") <= 0 ||
	    (msg = read_watcher(istate->watcher, NULL)) == NULL ||
	    strcmp(msg, "hello")) {
		reset_watches(istate, 1);
		return;
	}

	if (send_watcher(istate->watcher, "index %s %s",
			 sha1_to_hex(istate->sha1),
			 get_git_work_tree()) <= 0 ||
	    (msg = read_watcher(istate->watcher, NULL)) == NULL ||
	    strcmp(msg, "ok")) {
		reset_watches(istate, 0);
		return;
	}
}
