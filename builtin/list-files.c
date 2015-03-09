#include "cache.h"
#include "builtin.h"
#include "parse-options.h"
#include "pathspec.h"
#include "dir.h"
#include "quote.h"
#include "column.h"
#include "color.h"
#include "wt-status.h"

enum item_type {
	FROM_INDEX,
	FROM_WORKTREE,
	IS_DIR
};

struct item {
	enum item_type type;
	const char *path;
	char tag[2];
	const struct cache_entry *ce;
	struct stat st;
};

struct item_list {
	struct item *items;
	int nr, alloc;
	int tag_pos, tag_len;
};

static struct pathspec pathspec;
static struct pathspec recursive_pathspec;
static const char *prefix;
static int prefix_length;
static int show_tag = -1;
static unsigned int colopts;
static int max_depth;
static int show_dirs;
static int use_color = -1;
static int show_indicator;
static int show_cached, show_untracked;

static const char * const ls_usage[] = {
	N_("git list-files [options] [<pathspec>...]"),
	NULL
};

struct option ls_options[] = {
	OPT_GROUP(N_("Filter options")),
	OPT_BOOL('c', "cached", &show_cached,
		 N_("show cached files (default)")),
	OPT_BOOL('o', "others", &show_untracked,
		 N_("show untracked files")),

	OPT_GROUP(N_("Other")),
	OPT_BOOL(0, "tag", &show_tag, N_("show tags")),
	OPT_COLUMN('C', "column", &colopts, N_("show files in columns")),
	OPT_SET_INT('1', NULL, &colopts,
		    N_("shortcut for --no-column"), COL_PARSEOPT),
	{ OPTION_INTEGER, 0, "max-depth", &max_depth, N_("depth"),
	  N_("descend at most <depth> levels"), PARSE_OPT_NONEG,
	  NULL, 1 },
	OPT_SET_INT('R', "recursive", &max_depth,
		    N_("shortcut for --max-depth=-1"), -1),
	OPT__COLOR(&use_color, N_("show color")),
	OPT_BOOL('F', "classify", &show_indicator,
		 N_("append indicator (one of */=>@|) to entries")),
	OPT_END()
};

static int compare_item(const void *a_, const void *b_)
{
	const struct item *a = a_;
	const struct item *b = b_;
	return strcmp(a->path, b->path);
}

static void free_item(struct item *item)
{
	switch (item->type) {
	case IS_DIR:
		free((char*)item->path);
		break;
	default:
		break;
	}
}

static mode_t get_mode(const struct item *item)
{
	switch (item->type) {
	case IS_DIR:
		return S_IFDIR;
	case FROM_INDEX:
		return item->ce->ce_mode;
	case FROM_WORKTREE:
		return item->st.st_mode;
	}
	return S_IFREG;
}

static void remove_duplicates(struct item_list *list)
{
	int src, dst;

	if (list->nr <= 1)
		return;
	qsort(list->items, list->nr, sizeof(*list->items), compare_item);
	for (src = dst = 1; src < list->nr; src++) {
		if (!compare_item(list->items + dst - 1, list->items + src))
			free_item(list->items + src);
		else
			list->items[dst++] = list->items[src];
	}
	list->nr = dst;
}

static int add_directory(struct item_list *result,
			 const char *name)
{
	struct strbuf sb = STRBUF_INIT;
	struct item *item;
	const char *p;

	strbuf_addstr(&sb, name);
	while (sb.len && (p = strrchr(sb.buf, '/')) != NULL) {
		strbuf_setlen(&sb, p - sb.buf);
		if (!match_pathspec(&pathspec, sb.buf, sb.len, 0, NULL, 1))
			continue;

		ALLOC_GROW(result->items, result->nr + 1, result->alloc);
		item = result->items + result->nr++;
		item->type = IS_DIR;
		item->path = strbuf_detach(&sb, NULL);
		item->tag[0] = ' ';
		item->tag[1] = ' ';
		return 1;
	}
	strbuf_release(&sb);
	return 0;
}

static int matched(struct item_list *result, const char *name, int mode)
{
	int len = strlen(name);

	if (!match_pathspec(&recursive_pathspec, name, len, 0, NULL,
			    S_ISDIR(mode) || S_ISGITLINK(mode)))
		return 0;

	if (show_dirs && strchr(name, '/') &&
	    !match_pathspec(&pathspec, name, len, 0, NULL, 1) &&
	    add_directory(result, name))
		return 0;

	return 1;
}

static void populate_cached_entries(struct item_list *result,
				    const struct index_state *istate)
{
	int i;

	if (!show_cached)
		return;

	for (i = 0; i < istate->cache_nr; i++) {
		const struct cache_entry *ce = istate->cache[i];
		struct item *item;

		if (!matched(result, ce->name, ce->ce_mode))
			continue;

		ALLOC_GROW(result->items, result->nr + 1, result->alloc);
		item = result->items + result->nr++;
		item->type = FROM_INDEX;
		item->path = ce->name;
		item->tag[0] = ' ';
		item->tag[1] = ' ';
		item->ce = ce;
	}

	if (!show_dirs)
		return;
	remove_duplicates(result);
}

static void add_wt_item(struct item_list *result,
			enum item_type type,
			const char *path,
			const char *tag,
			const struct stat *st)
{
	struct item *item;

	ALLOC_GROW(result->items, result->nr + 1, result->alloc);
	item = result->items + result->nr++;
	item->type = type;
	item->path = path;
	memcpy(item->tag, tag, sizeof(item->tag));
	memcpy(&item->st, &st, sizeof(st));
}

static void populate_untracked(struct item_list *result,
			       const struct string_list *untracked)
{
	int i;

	for (i = 0; i < untracked->nr; i++) {
		const char *name = untracked->items[i].string;
		struct stat st;

		if (lstat(name, &st))
			/* color_filename() treats this as an orphan file */
			st.st_mode = 0;

		if (!matched(result, name, st.st_mode))
			continue;

		add_wt_item(result, FROM_WORKTREE, name, "??", &st);
	}
}

static void wt_status_populate(struct item_list *result,
			       struct index_state *istate)
{
	struct wt_status ws;

	if (!show_untracked)
		return;

	wt_status_prepare(&ws);
	copy_pathspec(&ws.pathspec, &recursive_pathspec);
	ws.relative_paths = 0;
	ws.use_color = 0;
	ws.fp = NULL;
	wt_status_collect(&ws);

	if (show_untracked)
		populate_untracked(result, &ws.untracked);

	remove_duplicates(result);
}

static void cleanup_tags(struct item_list *result)
{
	int i, same_1 = 1, same_2 = 1;

	if (!show_tag) {
		result->tag_pos = 0;
		result->tag_len = 0;
		return;
	}
	if (show_tag > 0 || show_cached + show_untracked > 1) {
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

static void append_indicator(struct strbuf *sb, mode_t mode)
{
	char c = 0;
	if (S_ISREG(mode)) {
		if (mode & (S_IXUSR | S_IXGRP | S_IXOTH))
			c = '*';
	} else if (S_ISDIR(mode))
		c = '/';
	else if (S_ISLNK(mode))
		c = '@';
	else if (S_ISFIFO(mode))
		c = '|';
	else if (S_ISSOCK(mode))
		c = '=';
	else if (S_ISGITLINK(mode))
		c = '&';
#ifdef S_ISDOOR
	else if (S_ISDOOR(mode))
		c = '>';
#endif
	if (c)
		strbuf_addch(sb, c);
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
		if (want_color(use_color)) {
			struct strbuf sb = STRBUF_INIT;
			strbuf_swap(&sb, &quoted);
			color_filename(&quoted, item->path, sb.buf,
				       get_mode(item), 1);
			strbuf_release(&sb);
		}
		if (show_indicator)
			append_indicator(&quoted, get_mode(item));
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
	if (!strcmp(var, "color.listfiles")) {
		use_color = git_config_colorbool(var, value);
		return 0;
	}
	return git_color_default_config(var, value, cb);
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

	if (!show_cached && !show_untracked)
		show_cached = 1;

	if (want_color(use_color))
		parse_ls_color();

	parse_pathspec(&pathspec, 0,
		       PATHSPEC_PREFER_CWD |
		       (max_depth != -1 ? PATHSPEC_MAXDEPTH_VALID : 0) |
		       PATHSPEC_STRIP_SUBMODULE_SLASH_CHEAP,
		       cmd_prefix, argv);
	pathspec.max_depth = max_depth;
	pathspec.recursive = 1;
	show_dirs = max_depth >= 0;
	copy_pathspec(&recursive_pathspec, &pathspec);
	recursive_pathspec.max_depth = -1;
	finalize_colopts(&colopts, -1);

	refresh_index(&the_index, REFRESH_QUIET | REFRESH_UNMERGED,
		      &recursive_pathspec, NULL, NULL);

	memset(&result, 0, sizeof(result));
	populate_cached_entries(&result, &the_index);
	wt_status_populate(&result, &the_index);
	cleanup_tags(&result);
	display(&result);
	/* free-ing result seems unnecessary */
	return 0;
}
