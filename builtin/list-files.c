#include "cache.h"
#include "builtin.h"
#include "parse-options.h"
#include "pathspec.h"
#include "dir.h"

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

static void add_one(struct string_list *result, const char *name)
{
	struct strbuf sb = STRBUF_INIT;
	struct string_list_item *item;

	strbuf_addstr(&sb, name);
	item = string_list_append(result, strbuf_detach(&sb, NULL));
	item->util = (char *)name;
}

static void populate_cached_entries(struct string_list *result,
				    const struct index_state *istate)
{
	int i;

	for (i = 0; i < istate->cache_nr; i++) {
		const struct cache_entry *ce = istate->cache[i];

		if (!match_pathspec(&pathspec, ce->name, ce_namelen(ce),
				    0, NULL,
				    S_ISDIR(ce->ce_mode) ||
				    S_ISGITLINK(ce->ce_mode)))
			continue;

		add_one(result, ce->name);
	}
}

static void display(const struct string_list *result)
{
	int i;

	for (i = 0; i < result->nr; i++) {
		const struct string_list_item *s = result->items + i;

		printf("%s\n", s->string);
	}
}

static int ls_config(const char *var, const char *value, void *cb)
{
	return git_default_config(var, value, cb);
}

int cmd_list_files(int argc, const char **argv, const char *cmd_prefix)
{
	struct string_list result = STRING_LIST_INIT_NODUP;

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

	populate_cached_entries(&result, &the_index);
	display(&result);
	string_list_clear(&result, 0);
	return 0;
}
