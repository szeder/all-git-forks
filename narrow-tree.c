#include "cache.h"
#include "diff.h"
#include "tree-walk.h"
#include "narrow-tree.h"
#include "pathspec.h"
#include "argv-array.h"
#include "tree-walk.h"

static struct pathspec narrow_pathspec;
static struct diff_options same_base_diffopts;

static void validate_prefix(const char *prefix, const char *prev_prefix)
{
	int len = strlen(prefix);

	if (!*prefix)
		die(_("empty line in $GIT_DIR/narrow"));

	if (prefix[len-1] == '/')
		die(_("trailing slash not allowed in $GIT_DIR: %s"), prefix);

	if (!prev_prefix)
		return;

	if (strcmp(prev_prefix, prefix) >= 0)
		die(_("$GIT_DIR/narrow is unsorted at %s"), prefix);
	len = strlen(prev_prefix);
	if (!strncmp(prev_prefix, prefix, len) && prefix[len] == '/')
		die(_("$GIT_DIR/narrow has nested prefix (%s and %s)"),
		    prev_prefix, prefix);
}

void check_narrow_prefix(void)
{
	struct strbuf sb = STRBUF_INIT;
	struct argv_array av = ARGV_ARRAY_INIT;
	char *p, *end;
	static int initialized;

	if (initialized)
		return;

	if (strbuf_read_file(&sb, git_path("narrow"), 0) < 0) {
		if (errno != ENOENT)
			die_errno(_("failed to read %s"), git_path("narrow"));
		return;
	}

	p = sb.buf;
	end = p + sb.len;
	while (p < end) {
		char *nl = strchr(p, '\n');

		if (!nl)
			nl = end;
		else
			*nl = '\0';
		validate_prefix(p, av.argc ? av.argv[av.argc - 1] : NULL);
		argv_array_push(&av, xstrdup(p));
		p = nl + 1;
	}
	strbuf_release(&sb);
	parse_pathspec(&narrow_pathspec,
		       PATHSPEC_ALL_MAGIC & ~PATHSPEC_LITERAL,
		       PATHSPEC_LITERAL_PATH,
		       "", av.argv);
	narrow_pathspec.recursive = 1;
	initialized = 1;

	diff_setup(&same_base_diffopts);
	DIFF_OPT_SET(&same_base_diffopts, RECURSIVE);
	DIFF_OPT_SET(&same_base_diffopts, QUICK);
	init_pathspec(&same_base_diffopts.pathspec, narrow_pathspec.raw);
	/*
	for (i = 0; i < narrow_pathspec.nr; i++)
		same_base_diffopts.pathspec.items[i].to_exclude = 1;
	same_base_diffopts.pathspec.include_by_default = 1;
	*/
}

char *get_narrow_string(void)
{
	struct strbuf sb = STRBUF_INIT;
	const char **prefix;

	check_narrow_prefix();
	prefix = narrow_pathspec._raw;
	if (prefix) {
		while (*prefix) {
			strbuf_addstr(&sb, *prefix);
			strbuf_addch(&sb, '\n');
			prefix++;
		}
	}
	return strbuf_detach(&sb, NULL);
}

struct pathspec *get_narrow_pathspec(void)
{
	check_narrow_prefix();
	return &narrow_pathspec;
}

int is_repository_narrow(void)
{
	struct pathspec *ps = get_narrow_pathspec();
	return ps && ps->nr;
}

/*
 * Given a source tree, find matching entries by path and replace
 * their hashes with ones from entries[]. entries[] must be sorted.
 * Return the new tree in "out".
 */
static int replace_subtrees(const char *in, unsigned long size,
			    struct strbuf *out,
			    int nr_entries, const struct name_entry *entries)
{
	const struct name_entry *new = entries;
	struct name_entry old;
	struct tree_desc desc;

	init_tree_desc(&desc, in, size);
	while (tree_entry(&desc, &old)) {
		const unsigned char *hash;

		strbuf_addf(out, "%o %.*s%c",
			    old.mode, tree_entry_len(&old),
			    old.path, '\0');
		if (nr_entries && !strcmp(old.path, new->path)) {
			hash = new->sha1;
			new++;
			nr_entries--;
		} else
			hash = old.sha1;
		strbuf_add(out, hash, GIT_SHA1_RAWSZ);
	}
	return 0;
}

/*
 * Take a valid cache tree, get more trees from narrow base. Narrow
 * extension must be available.
 */
int complete_cache_tree(struct index_state *istate)
{
}

int same_narrow_base(const unsigned char *t1, const unsigned char *t2)
{
	DIFF_OPT_CLR(&same_base_diffopts, HAS_CHANGES);
	return diff_tree_sha1(t1, t2, "", &same_base_diffopts) == 0 &&
		!DIFF_OPT_TST(&same_base_diffopts, HAS_CHANGES);
}
