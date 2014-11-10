#include "cache.h"
#include "watchman-support.h"
#include "strbuf.h"
#include <watchman.h>

static struct watchman_query *make_query(const char *last_update)
{
	struct watchman_query *query = watchman_query();
	watchman_query_set_fields(query, WATCHMAN_FIELD_NAME |
					 WATCHMAN_FIELD_EXISTS |
					 WATCHMAN_FIELD_NEWER);
	/* watchman_query_set_fields(query, WATCHMAN_FIELD_CCLOCK); */
	watchman_query_set_empty_on_fresh(query, 1);
	query->sync_timeout = core_watchman_sync_timeout;
	if (*last_update)
		watchman_query_set_since_oclock(query, last_update);
	return query;
}

static struct watchman_query_result* query_watchman(
	struct index_state *istate, struct watchman_connection *connection,
	const char *fs_path, const char *last_update)
{
	struct watchman_error wm_error;
	struct watchman_query *query;
	struct watchman_expression *expr;
	struct watchman_query_result *result;
	struct stat st;

	if (lstat(get_git_dir(), &st)) {
		/*
		 * Watchman gets confused if we delete the .git
		 * directory out from under it, since that's where it
		 * stores its cookies.  So we'll need to delete the
		 * watch and then recreate it. It's OK for this to
		 * fail, as the watch might have already been deleted.
		 */
		watchman_watch_del(connection, fs_path, &wm_error);

		if (watchman_watch(connection, fs_path, &wm_error)) {
			warning("Watchman watch error: %s", wm_error.message);
			return NULL;
		}
	}

	query = make_query(last_update);
	expr = watchman_true_expression();
	result = watchman_do_query(connection, fs_path, query, expr, &wm_error);
	watchman_free_query(query);
	watchman_free_expression(expr);

	if (!result)
		warning("Watchman query error: %s (at %s)",
			wm_error.message,
			*last_update ? last_update : "the beginning");

	return result;
}

static void update_index(struct index_state *istate,
			 struct watchman_query_result *result)
{
	int i;

	if (result->is_fresh_instance) {
		/* let refresh clear them later */
		for (i = 0; i < istate->cache_nr; i++)
			istate->cache[i]->ce_flags |= CE_NO_WATCH;
		goto done;
	}

	for (i = 0; i < result->nr; i++) {
		struct watchman_stat *wm = result->stats + i;
		int pos;

		if (!strncmp(wm->name, ".git/", 5) ||
		    strstr(wm->name, "/.git/"))
			continue;

		pos = index_name_pos(istate, wm->name, strlen(wm->name));
		if (pos < 0)
			continue;
		/* FIXME: ignore staged entries and gitlinks too? */

		istate->cache[pos]->ce_flags |= CE_NO_WATCH;
	}

	/*
	 * we have marked all modified files with CE_NO_WATCH
	 * according to watchman. All other files are supposed to be
	 * uptodate. Those with CE_NO_WATCH will be refreshed
	 * eventually and get that bit cleared.
	 */
	for (i = 0; i < istate->cache_nr; i++) {
		struct cache_entry *ce = istate->cache[i];
		if (ce_stage(ce) || (ce->ce_flags & CE_NO_WATCH))
			continue;
		ce_mark_uptodate(ce);
	}

done:
	free(istate->last_update);
	istate->last_update    = xstrdup(result->clock);
	istate->cache_changed |= WATCHMAN_CHANGED;
}

int check_watchman(struct index_state *istate)
{
	struct watchman_error wm_error;
	struct watchman_connection *connection;
	struct watchman_query_result *result;
	const char *fs_path;

	fs_path = get_git_work_tree();
	if (!fs_path)
		return -1;

	connection = watchman_connect(&wm_error);

	if (!connection) {
		warning("Watchman watch error: %s", wm_error.message);
		return -1;
	}

	result = query_watchman(istate, connection, fs_path, istate->last_update);
	watchman_connection_close(connection);
	if (!result)
		return -1;
	update_index(istate, result);
	watchman_free_query_result(result);
	return 0;
}
