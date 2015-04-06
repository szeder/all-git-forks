#include "cache.h"
#include "builtin.h"
#include "parse-options.h"
#include "pathspec.h"
#include "dir.h"
#include "quote.h"
#include "column.h"

enum item_type {
	FROM_INDEX
};

struct item {
	enum item_type type;
	const char *path;
	char tag[2];
	const struct cache_entry *ce;
};

struct item_list {
	struct item *items;
	int nr, alloc;
	int tag_pos, tag_len;
};

static struct pathspec pathspec;
static const char *prefix;
static int prefix_length;
static int show_tag = -1;
static unsigned int colopts;

static const char * const ls_usage[] = {
	N_("git list-files [options] [<pathspec>...]"),
	NULL
};

struct option ls_options[] = {
	OPT_BOOL(0, "tag", &show_tag, N_("show tags")),
	OPT_COLUMN('C', "column", &colopts, N_("show files in columns")),
	OPT_SET_INT('1', NULL, &colopts,
		    N_("shortcut for --no-column"), COL_PARSEOPT),
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
		item->tag[0] = ' ';
		item->tag[1] = ' ';
		item->ce = ce;
	}
}

static void cleanup_tags(struct item_list *result)
{
	int i, same_1 = 1, same_2 = 1;

	if (!show_tag) {
		result->tag_pos = 0;
		result->tag_len = 0;
		return;
	}
	if (show_tag > 0) {
		result->tag_pos = 0;
		result->tag_len = 2;
		return;
	}

	for (i = 1; i < result->nr && (same_1 || same_2); i++) {
		const char *s0 = result->items[i - 1].tag;
		const char *s1 = result->items[i].tag;

		same_1 = same_1 && s0[0] == s1[0];
		same_2 = same_2 && s0[1] == s1[1];
	}

	if (same_1 && same_2) {
		result->tag_pos = 0;
		result->tag_len = 0;
	} else if (same_1) {
		result->tag_pos = 1;
		result->tag_len = 1;
	} else if (same_2) {
		result->tag_pos = 0;
		result->tag_len = 1;
	} else {
		result->tag_pos = 0;
		result->tag_len = 2;
	}
}

/* this is similar to quote_path_relative() except it does not clear sb */
static void quote_item(struct strbuf *out, const struct item *item)
{
	static struct strbuf sb = STRBUF_INIT;
	const char *rel;

	strbuf_reset(&sb);
	rel = relative_path(item->path, prefix, &sb);
	quote_c_style(rel, out, NULL, 0);
}

static void display(const struct item_list *result)
{
	struct strbuf quoted = STRBUF_INIT;
	struct string_list s = STRING_LIST_INIT_DUP;
	int i;

	if (!prefix_length)
		prefix = NULL;

	for (i = 0; i < result->nr; i++) {
		const struct item *item = result->items + i;

		strbuf_reset(&quoted);
		if (result->tag_len) {
			strbuf_add(&quoted, item->tag + result->tag_pos,
				   result->tag_len);
			strbuf_addch(&quoted, ' ');
		}
		quote_item(&quoted, item);
		if (column_active(colopts))
			string_list_append(&s, quoted.buf);
		else
			printf("%s\n", quoted.buf);
	}

	if (column_active(colopts)) {
		struct column_options copts;
		memset(&copts, 0, sizeof(copts));
		copts.padding = 2;
		print_columns(&s, colopts, &copts);
		string_list_clear(&s, 0);
	}
	strbuf_release(&quoted);
}

static int ls_config(const char *var, const char *value, void *cb)
{
	if (starts_with(var, "column."))
		return git_column_config(var, value, "listfiles", &colopts);
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
	finalize_colopts(&colopts, -1);

	refresh_index(&the_index, REFRESH_QUIET | REFRESH_UNMERGED,
		      &pathspec, NULL, NULL);

	memset(&result, 0, sizeof(result));
	populate_cached_entries(&result, &the_index);
	cleanup_tags(&result);
	display(&result);
	/* free-ing result seems unnecessary */
	return 0;
}
