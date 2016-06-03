#include "builtin.h"
#include "cache.h"
#include "parse-options.h"
#include "bisect.h"
#include "refs.h"
#include "dir.h"

static GIT_PATH_FUNC(git_path_bisect_expected_rev, "BISECT_EXPECTED_REV")
static GIT_PATH_FUNC(git_path_bisect_ancestors_ok, "BISECT_ANCESTORS_OK")
static GIT_PATH_FUNC(git_path_bisect_log, "BISECT_LOG")
static GIT_PATH_FUNC(git_path_bisect_names, "BISECT_NAMES")
static GIT_PATH_FUNC(git_path_bisect_run, "BISECT_RUN")
static GIT_PATH_FUNC(git_path_bisect_terms, "BISECT_TERMS")
static GIT_PATH_FUNC(git_path_head_name, "head-name")
static GIT_PATH_FUNC(git_path_bisect_start, "BISECT_START")

static const char * const git_bisect_helper_usage[] = {
	N_("git bisect--helper --next-all [--no-checkout]"),
	N_("git bisect--helper --write-terms <bad_term> <good_term>"),
	N_("git bisect--helper --bisect-clean-state"),
	NULL
};

/*
 * Check whether the string `term` belongs to the set of strings
 * included in the variable arguments.
 */
static int one_of(const char *term, ...)
{
	int res = 0;
	va_list matches;
	const char *match;

	va_start(matches, term);
	while (!res && (match = va_arg(matches, const char *)))
		res = !strcmp(term, match);
	va_end(matches);

	return res;
}

static int check_term_format(const char *term, const char *orig_term)
{
	struct strbuf new_term = STRBUF_INIT;
	strbuf_addf(&new_term, "refs/bisect/%s", term);

	if (check_refname_format(new_term.buf, 0)) {
		strbuf_release(&new_term);
		return error(_("'%s' is not a valid term"), term);
	}
	strbuf_release(&new_term);

	if (one_of(term, "help", "start", "skip", "next", "reset",
			"visualize", "replay", "log", "run", NULL))
		return error(_("can't use the builtin command '%s' as a term"), term);

	/*
	 * In theory, nothing prevents swapping completely good and bad,
	 * but this situation could be confusing and hasn't been tested
	 * enough. Forbid it for now.
	 */

	if ((strcmp(orig_term, "bad") && one_of(term, "bad", "new", NULL)) ||
		 (strcmp(orig_term, "good") && one_of(term, "good", "old", NULL)))
		return error(_("can't change the meaning of the term '%s'"), term);

	return 0;
}

int write_terms(const char *bad, const char *good)
{
	struct strbuf content = STRBUF_INIT;
	FILE *fp;
	int res;

	if (!strcmp(bad, good))
		return error(_("please use two different terms"));

	if (check_term_format(bad, "bad") || check_term_format(good, "good"))
		return -1;

	strbuf_addf(&content, "%s\n%s\n", bad, good);
	fp = fopen(".git/BISECT_TERMS", "w");
	if (!fp){
		strbuf_release(&content);
		die_errno(_("could not open the file to read terms"));
	}
	res = strbuf_write(&content, fp);
	fclose(fp);
	strbuf_release(&content);
	return (res < 0) ? -1 : 0;
}

int mark_for_removal(const char *refname, const struct object_id *oid,
		       int flag, void *cb_data)
{
	struct string_list *refs = cb_data;
	char *ref = xstrfmt("refs/bisect/%s", refname);
	string_list_append(refs, ref);
	return 0;
}

int bisect_clean_state(void)
{
	int result = 0;
	struct string_list refs_for_removal = STRING_LIST_INIT_DUP;
	for_each_ref_in("refs/bisect/", mark_for_removal, (void *) &refs_for_removal);
	string_list_append(&refs_for_removal, "BISECT_HEAD");
	result |= delete_refs(&refs_for_removal);
	string_list_clear(&refs_for_removal, 0);
	remove_path(git_path_bisect_expected_rev());
	remove_path(git_path_bisect_ancestors_ok());
	remove_path(git_path_bisect_log());
	remove_path(git_path_bisect_names());
	remove_path(git_path_bisect_run());
	remove_path(git_path_bisect_terms());
	/* Cleanup head-name if it got left by an old version of git-bisect */
	remove_path(git_path_head_name());
	/* Cleanup BISECT_START last */
	remove_path(git_path_bisect_start());

	return result;
}

int cmd_bisect__helper(int argc, const char **argv, const char *prefix)
{
	enum {
		NEXT_ALL = 1,
		WRITE_TERMS,
		BISECT_CLEAN_STATE
	} cmdmode = 0;
	int no_checkout = 0;
	struct option options[] = {
		OPT_CMDMODE(0, "next-all", &cmdmode,
			 N_("perform 'git bisect next'"), NEXT_ALL),
		OPT_CMDMODE(0, "write-terms", &cmdmode,
			 N_("write the terms to .git/BISECT_TERMS"), WRITE_TERMS),
		OPT_CMDMODE(0, "bisect-clean-state", &cmdmode,
			 N_("cleanup the bisection state"), BISECT_CLEAN_STATE),
		OPT_BOOL(0, "no-checkout", &no_checkout,
			 N_("update BISECT_HEAD instead of checking out the current commit")),
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options,
			     git_bisect_helper_usage, 0);

	if (!cmdmode)
		usage_with_options(git_bisect_helper_usage, options);

	switch (cmdmode) {
	case NEXT_ALL:
		return bisect_next_all(prefix, no_checkout);
	case WRITE_TERMS:
		if (argc != 2)
			die(_("--write-terms requires two arguments"));
		return write_terms(argv[0], argv[1]);
	case BISECT_CLEAN_STATE:
		if (argc != 0)
			die(_("--bisect-clean-state requires no arguments"));
		return bisect_clean_state();
	default:
		die("BUG: unknown subcommand '%d'", cmdmode);
	}
	return 0;
}
