#include "cache.h"
#include "file-watcher-lib.h"
#include "pkt-line.h"
#include "unix-socket.h"
#include "string-list.h"

static char *watcher_path;
static int WAIT_TIME = 50;	/* in ms */
static int watch_lowerlimit = 65536;
static int recent_limit = 600;

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
	int i, changed = 0;
	if (istate->updated_entries) {
		string_list_clear(istate->updated_entries, 0);
		free(istate->updated_entries);
		istate->updated_entries = NULL;
	}
	for (i = 0; i < istate->cache_nr; i++)
		if (istate->cache[i]->ce_flags & CE_WATCHED) {
			istate->cache[i]->ce_flags &= ~(CE_WATCHED | CE_VALID);
			changed = 1;
		}
	recent_limit = 0;
	if (changed) {
		istate->update_watches = 1;
		istate->cache_changed = 1;
	}
	if (disconnect && istate->watcher > 0) {
		close(istate->watcher);
		istate->watcher = -1;
	}
}

static void mark_ce_valid(struct index_state *istate)
{
	struct strbuf sb = STRBUF_INIT;
	char *line, *end;
	int i, len;
	unsigned long n;
	if (packet_write_timeout(istate->watcher, WAIT_TIME, "get-changed") <= 0 ||
	    !(line = packet_read_line_timeout(istate->watcher, WAIT_TIME, &len)) ||
	    !starts_with(line, "changed ")) {
		reset_watches(istate, 1);
		return;
	}
	n = strtoul(line + 8, &end, 10);
	if (end != line + len) {
		reset_watches(istate, 1);
		return;
	}
	if (!n)
		goto done;
	strbuf_grow(&sb, n);
	if (read_in_full_timeout(istate->watcher, sb.buf, n, WAIT_TIME) != n) {
		strbuf_release(&sb);
		reset_watches(istate, 1);
		return;
	}
	line = sb.buf;
	end = line + n;
	for (; line < end; line += len + 1) {
		len = strlen(line);
		i = index_name_pos(istate, line, len);
		if (i < 0)
			continue;
		if (istate->cache[i]->ce_flags & CE_WATCHED) {
			istate->cache[i]->ce_flags &= ~CE_WATCHED;
			istate->cache_changed = 1;
		}
		if (!istate->updated_entries) {
			struct string_list *sl;
			sl = xmalloc(sizeof(*sl));
			memset(sl, 0, sizeof(*sl));
			sl->strdup_strings = 1;
			istate->updated_entries = sl;
		}
		string_list_append(istate->updated_entries, line);
	}
	strbuf_release(&sb);
done:
	for (i = 0; i < istate->cache_nr; i++)
		if (istate->cache[i]->ce_flags & CE_WATCHED)
			istate->cache[i]->ce_flags |= CE_VALID;
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

	if (!get_git_work_tree()) {
		reset_watches(istate, 1);
		return;
	}

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
		istate->update_watches = 1;
		return;
	}

	mark_ce_valid(istate);
}

static int sort_by_date(const void *a_, const void *b_)
{
	const struct cache_entry *a = *(const struct cache_entry **)a_;
	const struct cache_entry *b = *(const struct cache_entry **)b_;
	uint32_t seca = a->ce_stat_data.sd_mtime.sec;
	uint32_t secb = b->ce_stat_data.sd_mtime.sec;
	return seca - secb;
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
		(ce->ce_stat_data.sd_mtime.sec + recent_limit <= now);
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
	qsort(sorted, nr, sizeof(*sorted), sort_by_date);
	send_watches(istate, sorted, nr);
	free(sorted);
}

void close_watcher(struct index_state *istate, const unsigned char *sha1)
{
	struct strbuf sb = STRBUF_INIT;
	int len, i, nr;
	if (istate->watcher <= 0)
		return;
	if (packet_write_timeout(istate->watcher, WAIT_TIME,
				 "new-index %s", sha1_to_hex(sha1)) <= 0)
		goto done;
	nr = istate->updated_entries ? istate->updated_entries->nr : 0;
	if (!nr) {
		packet_write_timeout(istate->watcher, WAIT_TIME, "unchange 0");
		goto done;
	}
	for (i = len = 0; i < nr; i++) {
		const char *s = istate->updated_entries->items[i].string;
		len += strlen(s) + 1;
	}
	if (packet_write_timeout(istate->watcher, WAIT_TIME,
				 "unchange %d", len) <= 0)
	    goto done;
	strbuf_grow(&sb, len);
	for (i = 0; i < nr; i++) {
		const char *s = istate->updated_entries->items[i].string;
		int len = strlen(s);
		strbuf_add(&sb, s, len + 1);
	}
	/*
	 * it does not matter if it fails anymore, we're closing
	 * down. If it only gets through partially, file watcher
	 * should ignore it.
	 */
	write_in_full_timeout(istate->watcher, sb.buf, sb.len, WAIT_TIME);
	strbuf_release(&sb);
done:
	close(istate->watcher);
	istate->watcher = -1;
}
