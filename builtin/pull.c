/*
 * Builtin "git pull"
 *
 * Based on git-pull.sh by Junio C Hamano
 *
 * Fetch one or more remote refs and merge it/them into the current HEAD.
 */
#include "cache.h"
#include "builtin.h"
#include "parse-options.h"
#include "exec_cmd.h"
#include "run-command.h"

static const char * const pull_usage[] = {
	N_("git pull [options] [<repository> [<refspec>...]]"),
	NULL
};

/* Shared options */
static int opt_verbosity;
static struct strbuf opt_progress = STRBUF_INIT;

/* Options passed to git-merge */
static struct strbuf opt_diffstat = STRBUF_INIT;
static struct strbuf opt_log = STRBUF_INIT;
static struct strbuf opt_squash = STRBUF_INIT;
static struct strbuf opt_commit = STRBUF_INIT;
static struct strbuf opt_edit = STRBUF_INIT;
static struct strbuf opt_ff = STRBUF_INIT;
static struct strbuf opt_verify_signatures = STRBUF_INIT;
static struct argv_array opt_strategies = ARGV_ARRAY_INIT;
static struct argv_array opt_strategy_opts = ARGV_ARRAY_INIT;
static struct strbuf opt_gpg_sign = STRBUF_INIT;

static struct option pull_options[] = {
	/* Shared options */
	OPT__VERBOSITY(&opt_verbosity),
	{ OPTION_CALLBACK, 0, "progress", &opt_progress, NULL,
	  N_("force progress reporting"),
	  PARSE_OPT_NOARG, parse_opt_pass_strbuf},

	/* Options passed to git-merge */
	OPT_GROUP(N_("Options related to merging")),
	{ OPTION_CALLBACK, 'n', NULL, &opt_diffstat, NULL,
	  N_("do not show a diffstat at the end of the merge"),
	  PARSE_OPT_NOARG | PARSE_OPT_NONEG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 0, "stat", &opt_diffstat, NULL,
	  N_("show a diffstat at the end of the merge"),
	  PARSE_OPT_NOARG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 0, "summary", &opt_diffstat, NULL,
	  N_("(synonym to --stat)"),
	  PARSE_OPT_NOARG | PARSE_OPT_HIDDEN, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 0, "log", &opt_log, N_("n"),
	  N_("add (at most <n>) entries from shortlog to merge commit message"),
	  PARSE_OPT_OPTARG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 0, "squash", &opt_squash, NULL,
	  N_("create a single commit instead of doing a merge"),
	  PARSE_OPT_NOARG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 0, "commit", &opt_commit, NULL,
	  N_("perform a commit if the merge succeeds (default)"),
	  PARSE_OPT_NOARG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 0, "edit", &opt_edit, NULL,
	  N_("edit message before committing"),
	  PARSE_OPT_NOARG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 0, "ff", &opt_ff, NULL,
	  N_("allow fast-forward"),
	  PARSE_OPT_NOARG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 0, "ff-only", &opt_ff, NULL,
	  N_("abort if fast-forward is not possible"),
	  PARSE_OPT_NOARG | PARSE_OPT_NONEG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 0, "verify-signatures", &opt_verify_signatures, NULL,
	  N_("verify that the named commit has a valid GPG signature"),
	  PARSE_OPT_NOARG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 's', "strategy", &opt_strategies, N_("strategy"),
	  N_("merge strategy to use"),
	  0, parse_opt_pass_argv_array },
	{ OPTION_CALLBACK, 'X', "strategy-option", &opt_strategy_opts,
	  N_("option=value"),
	  N_("option for selected merge strategy"),
	  0, parse_opt_pass_argv_array },
	{ OPTION_CALLBACK, 'S', "gpg-sign", &opt_gpg_sign, N_("key-id"),
	  N_("GPG sign commit"),
	  PARSE_OPT_OPTARG, parse_opt_pass_strbuf },

	OPT_END()
};

/**
 * Pushes "-q" or "-v" switches into arr to match the opt_verbosity level.
 */
static void argv_push_verbosity(struct argv_array *arr)
{
	int verbosity;

	for (verbosity = opt_verbosity; verbosity > 0; verbosity--)
		argv_array_push(arr, "-v");

	for (verbosity = opt_verbosity; verbosity < 0; verbosity++)
		argv_array_push(arr, "-q");
}

/**
 * Parses argv into [<repo> [<refspecs>...]], returning their values in `repo`
 * as a string and `refspecs` as a null-terminated array of strings. If `repo`
 * is not provided in argv, it is set to NULL.
 */
static void parse_repo_refspecs(int argc, const char **argv, const char **repo,
		const char ***refspecs)
{
	if (argc > 0) {
		*repo = *argv++;
		argc--;
	} else
		*repo = NULL;
	*refspecs = argv;
}

/**
 * Runs git-fetch, returning its exit status. `repo` and `refspecs` are the
 * repository and refspecs to fetch, or NULL if they are not provided.
 */
static int run_fetch(const char *repo, const char **refspecs)
{
	struct argv_array args = ARGV_ARRAY_INIT;
	int ret;

	argv_array_pushl(&args, "fetch", "--update-head-ok", NULL);

	/* Shared options */
	argv_push_verbosity(&args);
	if (opt_progress.len)
		argv_array_push(&args, opt_progress.buf);

	if (repo)
		argv_array_push(&args, repo);
	while (*refspecs)
		argv_array_push(&args, *refspecs++);
	ret = run_command_v_opt(args.argv, RUN_GIT_CMD);
	argv_array_clear(&args);
	return ret;
}

/**
 * Runs git-merge, returning its exit status.
 */
static int run_merge(void)
{
	int ret;
	struct argv_array args = ARGV_ARRAY_INIT;

	argv_array_pushl(&args, "merge", NULL);

	/* Shared options */
	argv_push_verbosity(&args);
	if (opt_progress.len)
		argv_array_push(&args, opt_progress.buf);

	/* Options passed to git-merge */
	if (opt_diffstat.len)
		argv_array_push(&args, opt_diffstat.buf);
	if (opt_log.len)
		argv_array_push(&args, opt_log.buf);
	if (opt_squash.len)
		argv_array_push(&args, opt_squash.buf);
	if (opt_commit.len)
		argv_array_push(&args, opt_commit.buf);
	if (opt_edit.len)
		argv_array_push(&args, opt_edit.buf);
	if (opt_ff.len)
		argv_array_push(&args, opt_ff.buf);
	if (opt_verify_signatures.len)
		argv_array_push(&args, opt_verify_signatures.buf);
	argv_array_pushv(&args, opt_strategies.argv);
	argv_array_pushv(&args, opt_strategy_opts.argv);
	if (opt_gpg_sign.len)
		argv_array_push(&args, opt_gpg_sign.buf);

	argv_array_push(&args, "FETCH_HEAD");
	ret = run_command_v_opt(args.argv, RUN_GIT_CMD);
	argv_array_clear(&args);
	return ret;
}

int cmd_pull(int argc, const char **argv, const char *prefix)
{
	const char *repo, **refspecs;

	if (!getenv("_GIT_USE_BUILTIN_PULL")) {
		const char *path = mkpath("%s/git-pull", git_exec_path());

		if (sane_execvp(path, (char**) argv) < 0)
			die_errno("could not exec %s", path);
	}

	argc = parse_options(argc, argv, prefix, pull_options, pull_usage, 0);

	parse_repo_refspecs(argc, argv, &repo, &refspecs);

	if (run_fetch(repo, refspecs))
		return 1;

	return run_merge();
}
