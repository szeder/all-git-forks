#include "cache.h"
#include "file-watcher-lib.h"
#include "pkt-line.h"
#include "unix-socket.h"

static char *watcher_path;
static int WAIT_TIME = 50;	/* in ms */
static int watch_lowerlimit = 65536;
static int recent_limit = 1800;

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
	int i, changed = 0;
	for (i = 0; i < istate->cache_nr; i++)
		if (istate->cache[i]->ce_flags & CE_WATCHED) {
			istate->cache[i]->ce_flags &= ~CE_WATCHED;
			changed = 1;
		}
	if (changed) {
		istate->update_watches = 1;
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
	if (!strcmp(var, "filewatcher.minfiles")) {
		watch_lowerlimit = git_config_int(var, value);
		return 0;
	}
	if (!strcmp(var, "filewatcher.recentlimit")) {
		recent_limit = git_config_int(var, value);
		return 0;
	}
	return 0;
}

void open_watcher(struct index_state *istate)
{
	static int read_config = 0;
	char *msg;

	if (!read_config) {
		int i;
		/*
		 * can't hook into git_default_config because
		 * read_cache() may be called even before git_config()
		 * call.
		 */
		git_config(watcher_config, NULL);
		for (i = 0; i < istate->cache_nr; i++)
			if (istate->cache[i]->ce_flags & CE_WATCHED)
				break;
		if (i == istate->cache_nr)
			recent_limit = 0;
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
		istate->update_watches = 1;
		return;
	}
}

static int sort_by_date(const void *a_, const void *b_)
{
	const struct cache_entry *a = *(const struct cache_entry **)a_;
	const struct cache_entry *b = *(const struct cache_entry **)b_;
	uint32_t seca = a->ce_stat_data.sd_mtime.sec;
	uint32_t secb = b->ce_stat_data.sd_mtime.sec;
	return seca - secb;
}

static int do_watch_entries(struct index_state *istate,
			    struct cache_entry **cache,
			    struct strbuf *sb, int start, int now)
{
	char *line, *end;
	int i, len;
	long n;

	if (send_watcher(istate->watcher, "%s", sb->buf) <= 0 ||
	    (line = read_watcher(istate->watcher, &len)) == NULL ||
	    !starts_with(line, "watched "))
		return -1;
	n = strtoul(line + 8, &end, 10);
	if (end != line + len || start + n > now)
		return -1;
	for (i = 0; i < n; i++)
		cache[start + i]->ce_flags |= CE_WATCHED;
	istate->cache_changed = 1;
	if (start + n < now)
		return i;
	strbuf_reset(sb);
	strbuf_addstr(sb, "watch ");
	return 0;
}

static inline int ce_watchable(struct cache_entry *ce, time_t now)
{
	return
		!(ce->ce_flags & CE_WATCHED) &&
		!(ce->ce_flags & CE_VALID) &&
		/*
		 * S_IFGITLINK should not be watched
		 * obviously. S_IFLNK could be problematic because
		 * inotify may follow symlinks without IN_DONT_FOLLOW
		 */
		S_ISREG(ce->ce_mode) &&
		(ce->ce_stat_data.sd_mtime.sec + recent_limit < now);
}

void watch_entries(struct index_state *istate)
{
	int i, start, nr;
	struct cache_entry **sorted;
	struct strbuf sb = STRBUF_INIT;
	int val, ret;
	socklen_t vallen = sizeof(val);
	time_t now = time(NULL);

	if (istate->watcher <= 0 || !istate->update_watches)
		return;
	istate->update_watches = 0;
	istate->cache_changed = 1;
	for (i = nr = 0; i < istate->cache_nr; i++)
		if (ce_watchable(istate->cache[i], now))
			nr++;
	if (nr < watch_lowerlimit)
		return;
	sorted = xmalloc(sizeof(*sorted) * nr);
	for (i = nr = 0; i < istate->cache_nr; i++)
		if (ce_watchable(istate->cache[i], now))
			sorted[nr++] = istate->cache[i];

	getsockopt(istate->watcher, SOL_SOCKET, SO_SNDBUF, &val, &vallen);
	if (val > 65520)
		val = 65520;

	strbuf_grow(&sb, val);
	strbuf_addstr(&sb, "watch ");

	qsort(sorted, nr, sizeof(*sorted), sort_by_date);
	for (i = start = 0; i < nr; i++) {
		if (sb.len + 4 + ce_namelen(sorted[i]) >= val &&
		    (ret = do_watch_entries(istate, sorted, &sb, start, i))) {
			if (ret < 0)
				reset_watches(istate, 1);
			break;
		}
		packet_buf_write_notrace(&sb, "%s", sorted[i]->name);
	}
	if (i == nr && start < nr &&
	    do_watch_entries(istate, sorted, &sb, start, nr) < 0)
		reset_watches(istate, 1);
	strbuf_release(&sb);
	free(sorted);
}
