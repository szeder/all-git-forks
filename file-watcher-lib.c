#include "cache.h"
#include "file-watcher-lib.h"
#include "pkt-line.h"
#include "unix-socket.h"

static char *watcher_path;
static int WAIT_TIME = 50;	/* in ms */

static int connect_watcher(const char *path)
{
	struct strbuf sb = STRBUF_INIT;
	int fd;

	if (!path || !*path)
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
			istate->cache[i]->ce_flags &= ~(CE_WATCHED | CE_VALID);
			istate->cache_changed = 1;
		}
	if (disconnect && istate->watcher > 0) {
		close(istate->watcher);
		istate->watcher = -1;
	}
}

static int watcher_config(const char *var, const char *value, void *data)
{
	if (!strcmp(var, "filewatcher.path")) {
		if (is_absolute_path(value))
			watcher_path = xstrdup(value);
		else if (*value == '~')
			watcher_path = expand_user_path(value);
		else
			watcher_path = git_pathdup("%s", value);
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

	if (!get_git_work_tree()) {
		reset_watches(istate, 1);
		return;
	}

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
	if (packet_write_timeout(istate->watcher, WAIT_TIME, "hello") <= 0 ||
	    (msg = packet_read_line_timeout(istate->watcher, WAIT_TIME, NULL)) == NULL ||
	    strcmp(msg, "hello")) {
		reset_watches(istate, 1);
		return;
	}

	if (packet_write_timeout(istate->watcher, WAIT_TIME, "index %s %s",
				 sha1_to_hex(istate->sha1),
				 get_git_work_tree()) <= 0 ||
	    (msg = packet_read_line_timeout(istate->watcher, WAIT_TIME, NULL)) == NULL ||
	    strcmp(msg, "ok")) {
		reset_watches(istate, 0);
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

static inline int ce_watchable(struct cache_entry *ce)
{
	return
		!(ce->ce_flags & CE_WATCHED) &&
		!(ce->ce_flags & CE_VALID) &&
		/*
		 * S_IFGITLINK should not be watched
		 * obviously. S_IFLNK could be problematic because
		 * inotify may follow symlinks without IN_DONT_FOLLOW
		 */
		S_ISREG(ce->ce_mode);
}

static void send_watches(struct index_state *istate,
			 struct cache_entry **sorted, int nr)
{
	struct strbuf sb = STRBUF_INIT;
	int i, len = 0;

	for (i = 0; i < nr; i++)
		len += ce_namelen(sorted[i]) + 1;

	if (packet_write_timeout(istate->watcher, WAIT_TIME, "watch %d", len) <= 0)
		return;

	strbuf_grow(&sb, len);
	for (i = 0; i < nr; i++)
		strbuf_add(&sb, sorted[i]->name, ce_namelen(sorted[i]) + 1);

	if (write_in_full_timeout(istate->watcher, sb.buf,
				  sb.len, WAIT_TIME) != sb.len) {
		strbuf_release(&sb);
		return;
	}
	strbuf_release(&sb);

	for (;;) {
		char *line, *end;
		unsigned long n;

		if (!(line = packet_read_line_timeout(istate->watcher,
						      WAIT_TIME, &len)))
			return;
		if (starts_with(line, "watching "))
			continue;
		if (!starts_with(line, "watched "))
			return;
		n = strtoul(line + 8, &end, 10);
		for (i = 0; i < n; i++)
			sorted[i]->ce_flags |= CE_WATCHED;
		istate->cache_changed = 1;
		break;
	}
}

void watch_entries(struct index_state *istate)
{
	int i, nr;
	struct cache_entry **sorted;

	if (istate->watcher <= 0)
		return;
	for (i = nr = 0; i < istate->cache_nr; i++)
		if (ce_watchable(istate->cache[i]))
			nr++;
	sorted = xmalloc(sizeof(*sorted) * nr);
	for (i = nr = 0; i < istate->cache_nr; i++)
		if (ce_watchable(istate->cache[i]))
			sorted[nr++] = istate->cache[i];
	qsort(sorted, nr, sizeof(*sorted), sort_by_date);
	send_watches(istate, sorted, nr);
	free(sorted);
}
