#include "cache.h"
#include "builtin.h"
#include "parse-options.h"
#include "pathspec.h"
#include "dir.h"
#include "quote.h"
#include "column.h"
#include "color.h"
#include "wt-status.h"

static struct pathspec pathspec;
static struct pathspec recursive_pathspec;
static const char *prefix;
static int prefix_length;
static unsigned int colopts;
static int max_depth;
static int show_dirs;
static int use_color = -1;
static int show_indicator;
static int show_cached, show_untracked;
static int show_ignored;

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
	OPT_BOOL('i', "ignored", &show_ignored,
		 N_("show ignored files")),

	OPT_GROUP(N_("Other")),
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

static void add_one(struct string_list *result, const char *name, int mode,
		    const char *tag)
{
	struct strbuf sb = STRBUF_INIT;
	struct string_list_item *item;

	quote_path_relative(name, prefix_length ? prefix : NULL, &sb);
	if (want_color(use_color)) {
		struct strbuf quoted = STRBUF_INIT;
		strbuf_swap(&sb, &quoted);
		color_filename(&sb, name, quoted.buf, mode, 1);
		strbuf_release(&quoted);
	}
	if (show_indicator)
		append_indicator(&sb, mode);
	strbuf_insert(&sb, 0, "   ", 3);
	sb.buf[0] = tag[0];
	sb.buf[1] = tag[1];
	item = string_list_append(result, strbuf_detach(&sb, NULL));
	item->util = (char *)name;
}

static int add_directory(struct string_list *result,
			 const char *name)
{
	struct strbuf sb = STRBUF_INIT;
	const char *p;

	strbuf_add(&sb, name, strlen(name));
	while (sb.len && (p = strrchr(sb.buf, '/')) != NULL) {
		strbuf_setlen(&sb, p - sb.buf);
		if (!match_pathspec(&pathspec, sb.buf, sb.len, 0, NULL, 1))
			continue;
		add_one(result, sb.buf, S_IFDIR, "  ");
		/*
		 * sb.buf is leaked, but because this command is
		 * short-lived anyway so it does not matter much
		 */
		return 1;
	}
	strbuf_release(&sb);
	return 0;
}

static int matched(struct string_list *result, const char *name, int mode)
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

static int compare_output(const void *a_, const void *b_)
{
	const struct string_list_item *a = a_;
	const struct string_list_item *b = b_;
	return strcmp(a->util, b->util);
}

static void populate_cached_entries(struct string_list *result,
				    const struct index_state *istate)
{
	int i;

	if (!show_cached)
		return;

	for (i = 0; i < istate->cache_nr; i++) {
		const struct cache_entry *ce = istate->cache[i];

		if (!matched(result, ce->name, ce->ce_mode))
			continue;

		add_one(result, ce->name, ce->ce_mode, "  ");
	}

	if (!show_dirs)
		return;
	qsort(result->items, result->nr, sizeof(*result->items), compare_output);
	string_list_remove_duplicates(result, 0);
}

static void populate_untracked(struct string_list *result,
			       const struct string_list *untracked,
			       const char *tag)
{
	int i;

	for (i = 0; i < untracked->nr; i++) {
		const struct string_list_item *item = untracked->items + i;
		const char *name = item->string;
		struct stat st;

		if (lstat(name, &st))
			/* color_filename() treats this as an orphan file */
			st.st_mode = 0;

		if (!matched(result, name, st.st_mode))
			continue;

		add_one(result, name, st.st_mode, tag);
	}
}

static void wt_status_populate(struct string_list *result,
			       struct index_state *istate)
{
	struct wt_status ws;

	if (!show_untracked && !show_ignored)
		return;

	wt_status_prepare(&ws);
	copy_pathspec(&ws.pathspec, &recursive_pathspec);
	if (show_ignored)
		ws.show_ignored_files = 1;
	ws.relative_paths = 0;
	ws.use_color = 0;
	ws.fp = NULL;
	wt_status_collect(&ws);

	if (show_untracked)
		populate_untracked(result, &ws.untracked, "??");
	if (show_ignored)
		populate_untracked(result, &ws.ignored, "!!");

	qsort(result->items, result->nr, sizeof(*result->items), compare_output);
	string_list_remove_duplicates(result, 0);
}

static void cleanup_tags(struct string_list *result)
{
	int i, same_1 = 1, same_2 = 1, pos, len;

	if (show_cached + show_untracked + show_ignored > 1)
		return;

	for (i = 1; i < result->nr && (same_1 || same_2); i++) {
		const char *s0 = result->items[i - 1].string;
		const char *s1 = result->items[i].string;

		same_1 = same_1 && s0[0] == s1[0];
		same_2 = same_2 && s0[1] == s1[1];
	}

	if (same_1 && same_2) {
		pos = 0;
		len = 3;
	} else if (same_1) {
		pos = 0;
		len = 1;
	} else if (same_2) {
		pos = 1;
		len = 1;
	} else
		return;

	for (i = 0; i < result->nr; i++) {
		char *s = result->items[i].string;
		int length = strlen(s);
		memmove(s + pos, s + pos + len, length - len + 1);
	}
}

static void display(const struct string_list *result)
{
	int i;

	if (column_active(colopts)) {
		struct column_options copts;
		memset(&copts, 0, sizeof(copts));
		copts.padding = 2;
		print_columns(result, colopts, &copts);
		return;
	}

	for (i = 0; i < result->nr; i++) {
		const struct string_list_item *s = result->items + i;

		printf("%s\n", s->string);
	}
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
	struct string_list result = STRING_LIST_INIT_NODUP;

	setenv(GIT_GLOB_PATHSPECS_ENVIRONMENT, "1", 0);

	if (argc == 2 && !strcmp(argv[1], "-h"))
		usage_with_options(ls_usage, ls_options);

	prefix = cmd_prefix;
	if (prefix)
		prefix_length = strlen(prefix);

	if (read_cache() < 0)
		die(_("index file corrupt"));

	git_config(ls_config, NULL);

	argc = parse_options(argc, argv, prefix, ls_options, ls_usage, 0);

	if (!show_cached && !show_untracked && !show_ignored)
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

	populate_cached_entries(&result, &the_index);
	wt_status_populate(&result, &the_index);
	cleanup_tags(&result);
	display(&result);
	string_list_clear(&result, 0);
	return 0;
}
