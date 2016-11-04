/*
 * GIT - The information manager from hell
 */

#include "cache.h"
#include "refs.h"
#include "builtin.h"
#include "strbuf.h"

static const char builtin_check_ref_format_usage[] =
"git check-ref-format [--normalize] [<options>] <refname>\n"
"   or: git check-ref-format [<options>] --branch <branchname-shorthand>";

/*
 * Return a copy of refname but with leading slashes removed and runs
 * of adjacent slashes replaced with single slashes.
 *
 * This function is similar to normalize_path_copy(), but stripped down
 * to meet check_ref_format's simpler needs.
 */
static char *collapse_slashes(const char *refname)
{
	char *ret = xmallocz(strlen(refname));
	char ch;
	char prev = '/';
	char *cp = ret;

	while ((ch = *refname++) != '\0') {
		if (prev == '/' && ch == prev)
			continue;

		*cp++ = ch;
		prev = ch;
	}
	*cp = '\0';
	return ret;
}

static int check_ref_format_branch(const char *arg)
{
	struct strbuf sb = STRBUF_INIT;
	int nongit;

	setup_git_directory_gently(&nongit);
	if (strbuf_check_branch_ref(&sb, arg))
		die("'%s' is not a valid branch name", arg);
	printf("%s\n", sb.buf + 11);
	return 0;
}

static int normalize = 0;
static int check_branch = 0;
static int flags = 0;
static int report_errors = 0;

static int check_one_ref_format(const char *refname)
{
	int got;

	if (normalize)
		refname = collapse_slashes(refname);
	got = check_branch
		? check_ref_format_branch(refname)
		: check_refname_format(refname, flags);
	if (got) {
		if (report_errors)
			fprintf(stderr, "bad ref format: %s\n", refname);
		return 1;
	}
	if (normalize) {
		printf("%s\n", refname);
		free((void*)refname);
	}
}

int cmd_check_ref_format(int argc, const char **argv, const char *prefix)
{
	int i;

	if (argc == 2 && !strcmp(argv[1], "-h"))
		usage(builtin_check_ref_format_usage);

	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		if (!strcmp(argv[i], "--normalize") || !strcmp(argv[i], "--print"))
			normalize = 1;
		else if (!strcmp(argv[i], "--allow-onelevel"))
			flags |= REFNAME_ALLOW_ONELEVEL;
		else if (!strcmp(argv[i], "--no-allow-onelevel"))
			flags &= ~REFNAME_ALLOW_ONELEVEL;
		else if (!strcmp(argv[i], "--refspec-pattern"))
			flags |= REFNAME_REFSPEC_PATTERN;
		else if (!strcmp(argv[i], "--branch"))
			check_branch = 1;
		else if (!strcmp(argv[i], "--report-errors"))
			report_errors = 1;
		else
			usage(builtin_check_ref_format_usage);
	}

	if (check_branch && (flags || normalize))
		usage(builtin_check_ref_format_usage);

	if (! (i == argc - 1))
		usage(builtin_check_ref_format_usage);

	return check_one_ref_format(argv[i]);
}
