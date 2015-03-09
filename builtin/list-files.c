#include "cache.h"
#include "builtin.h"
#include "parse-options.h"
#include "pathspec.h"
#include "dir.h"

enum item_type {
	FROM_INDEX
};

struct item {
	enum item_type type;
	const char *path;
	const struct cache_entry *ce;
};

struct item_list {
	struct item *items;
	int nr, alloc;
};

static struct pathspec pathspec;
static const char *prefix;
static int prefix_length;

static const char * const ls_usage[] = {
	N_("git list-files [options] [<pathspec>...]"),
	NULL
};

struct option ls_options[] = {
	OPT_END()
};

static void populate_cached_entries(struct item_list *result,
				    const struct index_state *istate)
{
	int i;

	for (i = 0; i < istate->cache_nr; i++) {
		const struct cache_entry *ce = istate->cache[i];
		struct item *item;

		if (!match_pathspec(&pathspec, ce->name, ce_namelen(ce),
				    0, NULL,
				    S_ISDIR(ce->ce_mode) ||
				    S_ISGITLINK(ce->ce_mode)))
			continue;

		ALLOC_GROW(result->items, result->nr + 1, result->alloc);
		item = result->items + result->nr++;
		item->type = FROM_INDEX;
		item->path = ce->name;
		item->ce = ce;
	}
}

static void display(const struct item_list *result)
{
	int i;

	for (i = 0; i < result->nr; i++) {
		const struct item *item = result->items + i;

		printf("%s\n", item->path);
	}
}

static int ls_config(const char *var, const char *value, void *cb)
{
	return git_default_config(var, value, cb);
}

int cmd_list_files(int argc, const char **argv, const char *cmd_prefix)
{
	struct item_list result;

	if (argc == 2 && !strcmp(argv[1], "-h"))
		usage_with_options(ls_usage, ls_options);

	prefix = cmd_prefix;
	if (prefix)
		prefix_length = strlen(prefix);

	if (read_cache() < 0)
		die(_("index file corrupt"));

	git_config(ls_config, NULL);

	argc = parse_options(argc, argv, prefix, ls_options, ls_usage, 0);

	parse_pathspec(&pathspec, 0,
		       PATHSPEC_PREFER_CWD |
		       PATHSPEC_STRIP_SUBMODULE_SLASH_CHEAP,
		       cmd_prefix, argv);
	pathspec.max_depth = 0;
	pathspec.recursive = 1;

	refresh_index(&the_index, REFRESH_QUIET | REFRESH_UNMERGED,
		      &pathspec, NULL, NULL);

	memset(&result, 0, sizeof(result));
	populate_cached_entries(&result, &the_index);
	display(&result);
	/* free-ing result seems unnecessary */
	return 0;
}
