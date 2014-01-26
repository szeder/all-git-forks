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
