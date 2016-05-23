#include "builtin.h"
#include "cache.h"
#include "parse-options.h"
#include "bisect.h"
#include "refs.h"

static const char * const git_bisect_helper_usage[] = {
	N_("git bisect--helper --next-all [--no-checkout]"),
	N_("git bisect--helper --write-terms <bad_term> <good_term>"),
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
	fp = fopen(git_path("BISECT_TERMS"), "w");
	printf("Successfully opened a file.\n");
	if (!fp){
		strbuf_release(&content);
		printf("fuck. couldn't open file\n");
		die_errno(_("could not open the file to read terms"));
	}
	res = strbuf_write(&content, fp);
	printf("Yes I could write to the file with code %d\n", res);
	fclose(fp);
	strbuf_release(&content);
	return (res < 0) ? -1 : 0;
}
int cmd_bisect__helper(int argc, const char **argv, const char *prefix)
{
	enum {
		NEXT_ALL = 1,
		WRITE_TERMS
	} cmdmode = 0;
	int no_checkout = 0;
	struct option options[] = {
		OPT_CMDMODE(0, "next-all", &cmdmode,
			 N_("perform 'git bisect next'"), NEXT_ALL),
		OPT_CMDMODE(0, "write-terms", &cmdmode,
			 N_("write the terms to .git/BISECT_TERMS"), WRITE_TERMS),
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
	default:
		die("BUG: unknown subcommand '%d'", cmdmode);
	}
	return 0;
}
