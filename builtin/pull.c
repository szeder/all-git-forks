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
#include "sha1-array.h"
#include "remote.h"

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

/* Options passed to git-fetch */
static struct strbuf opt_all = STRBUF_INIT;
static struct strbuf opt_append = STRBUF_INIT;
static struct strbuf opt_upload_pack = STRBUF_INIT;
static int opt_force;
static struct strbuf opt_tags = STRBUF_INIT;
static struct strbuf opt_prune = STRBUF_INIT;
static struct strbuf opt_recurse_submodules = STRBUF_INIT;
static int opt_dry_run;
static struct strbuf opt_keep = STRBUF_INIT;
static struct strbuf opt_depth = STRBUF_INIT;
static struct strbuf opt_unshallow = STRBUF_INIT;
static struct strbuf opt_update_shallow = STRBUF_INIT;
static struct strbuf opt_refmap = STRBUF_INIT;

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

	/* Options passed to git-fetch */
	OPT_GROUP(N_("Options related to fetching")),
	{ OPTION_CALLBACK, 0, "all", &opt_all, 0,
	  N_("fetch from all remotes"),
	  PARSE_OPT_NOARG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 'a', "append", &opt_append, 0,
	  N_("append to .git/FETCH_HEAD instead of overwriting"),
	  PARSE_OPT_NOARG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 0, "upload-pack", &opt_upload_pack, N_("path"),
	  N_("path to upload pack on remote end"),
	  0, parse_opt_pass_strbuf },
	OPT__FORCE(&opt_force, N_("force overwrite of local branch")),
	{ OPTION_CALLBACK, 't', "tags", &opt_tags, 0,
	  N_("fetch all tags and associated objects"),
	  PARSE_OPT_NOARG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 'p', "prune", &opt_prune, 0,
	  N_("prune remote-tracking branches no longer on remote"),
	  PARSE_OPT_NOARG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 0, "recurse-submodules", &opt_recurse_submodules,
	  N_("on-demand"),
	  N_("control recursive fetching of submodules"),
	  PARSE_OPT_OPTARG, parse_opt_pass_strbuf },
	OPT_BOOL(0, "dry-run", &opt_dry_run,
		N_("dry run")),
	{ OPTION_CALLBACK, 'k', "keep", &opt_keep, 0,
	  N_("keep downloaded pack"),
	  PARSE_OPT_NOARG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 0, "depth", &opt_depth, N_("depth"),
	  N_("deepen history of shallow clone"),
	  0, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 0, "unshallow", &opt_unshallow, 0,
	  N_("convert to a complete repository"),
	  PARSE_OPT_NONEG | PARSE_OPT_NOARG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 0, "update-shallow", &opt_update_shallow, 0,
	  N_("accept refs that update .git/shallow"),
	  PARSE_OPT_NOARG, parse_opt_pass_strbuf },
	{ OPTION_CALLBACK, 0, "refmap", &opt_refmap, N_("refmap"),
	  N_("specify fetch refmap"),
	  PARSE_OPT_NONEG, parse_opt_pass_strbuf },

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
 * Pushes "-f" switches into arr to match the opt_force level.
 */
static void argv_push_force(struct argv_array *arr)
{
	int force = opt_force;
	while (force-- > 0)
		argv_array_push(arr, "-f");
}

/**
 * If pull.ff is "true", sets sb to "--ff". If pull.ff is "false", sets sb to
 * "--no-ff". If pull.ff is "only", sets sb to "--ff-only". If pull.ff is
 * set to an invalid value, die with an error.
 */
static void config_get_ff(struct strbuf *sb)
{
	const char *value;

	if (git_config_get_value("pull.ff", &value))
		return;
	switch (git_config_maybe_bool("pull.ff", value)) {
		case 0:
			strbuf_addstr(sb, "--no-ff");
			return;
		case 1:
			strbuf_addstr(sb, "--ff");
			return;
	}
	if (!strcmp(value, "only")) {
		strbuf_addstr(sb, "--ff-only");
		return;
	}
	die(_("Invalid value for pull.ff: %s"), value);
}

/**
 * Appends merge candidates from FETCH_HEAD that are not marked not-for-merge
 * into merge_heads.
 */
static void get_merge_heads(struct sha1_array *merge_heads)
{
	const char *filename = git_path("FETCH_HEAD");
	FILE *fp;
	struct strbuf sb = STRBUF_INIT;
	unsigned char sha1[GIT_SHA1_RAWSZ];

	if (!(fp = fopen(filename, "r")))
		die_errno(_("could not open '%s' for reading"), filename);
	while(strbuf_getline(&sb, fp, '\n') != EOF) {
		if (get_sha1_hex(sb.buf, sha1))
			continue;  /* invalid line: does not start with SHA1 */
		if (starts_with(sb.buf + GIT_SHA1_HEXSZ, "\tnot-for-merge"))
			continue;  /* ref is not-for-merge */
		sha1_array_append(merge_heads, sha1);
	}
	fclose(fp);
	strbuf_release(&sb);
}

/**
 * Used by die_no_merge_candidates() as a for_each_remote() callback to
 * retrieve the name of the remote if the repository only has one remote.
 */
static int get_only_remote(struct remote *remote, void *cb_data)
{
	const char **remote_name = cb_data;

	if (*remote_name)
		return -1;

	*remote_name = remote->name;
	return 0;
}

/**
 * Dies with the appropriate reason for why there are no merge candidates:
 *
 * 1. We fetched from a specific remote, and a refspec was given, but it ended
 *    up not fetching anything. This is usually because the user provided a
 *    wildcard refspec which had no matches on the remote end.
 *
 * 2. We fetched from a non-default remote, but didn't specify a branch to
 *    merge. We can't use the configured one because it applies to the default
 *    remote, thus the user must specify the branches to merge.
 *
 * 3. We fetched from the branch's or repo's default remote, but:
 *
 *    a. We are not on a branch, so there will never be a configured branch to
 *       merge with.
 *
 *    b. We are on a branch, but there is no configured branch to merge with.
 *
 * 4. We fetched from the branch's or repo's default remote, but the configured
 *    branch to merge didn't get fetched. (Either it doesn't exist, or wasn't
 *    part of the configured fetch refspec.)
 */
static void NORETURN die_no_merge_candidates(const char *repo, const char **refspecs)
{
	struct branch *curr_branch = branch_get("HEAD");
	const char *remote = curr_branch ? curr_branch->remote_name : NULL;

	if (*refspecs) {
		fprintf_ln(stderr, _("There are no candidates for merging among the refs that you just fetched."));
		fprintf_ln(stderr, _("Generally this means that you provided a wildcard refspec which had no\n"
					"matches on the remote end."));
	} else if (repo && curr_branch && (!remote || strcmp(repo, remote))) {
		fprintf_ln(stderr, _("You asked to pull from the remote '%s', but did not specify\n"
			"a branch. Because this is not the default configured remote\n"
			"for your current branch, you must specify a branch on the command line."),
			repo);
	} else if (!curr_branch) {
		fprintf_ln(stderr, _("You are not currently on a branch."));
		fprintf_ln(stderr, _("Please specify which branch you want to merge with."));
		fprintf_ln(stderr, _("See git-pull(1) for details."));
		fprintf(stderr, "\n");
		fprintf_ln(stderr, "    git pull <remote> <branch>");
		fprintf(stderr, "\n");
	} else if (!curr_branch->merge_nr) {
		const char *remote_name = NULL;

		if (for_each_remote(get_only_remote, &remote_name) || !remote_name)
			remote_name = "<remote>";

		fprintf_ln(stderr, _("There is no tracking information for the current branch."));
		fprintf_ln(stderr, _("Please specify which branch you want to merge with."));
		fprintf_ln(stderr, _("See git-pull(1) for details."));
		fprintf(stderr, "\n");
		fprintf_ln(stderr, "    git pull <remote> <branch>");
		fprintf(stderr, "\n");
		fprintf_ln(stderr, _("If you wish to set tracking information for this branch you can do so with:\n"
				"\n"
				"    git branch --set-upstream-to=%s/<branch> %s\n"),
				remote_name, curr_branch->name);
	} else
		fprintf_ln(stderr, _("Your configuration specifies to merge with the ref '%s'\n"
			"from the remote, but no such ref was fetched."),
			*curr_branch->merge_name);
	exit(1);
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

	/* Options passed to git-fetch */
	if (opt_all.len)
		argv_array_push(&args, opt_all.buf);
	if (opt_append.len)
		argv_array_push(&args, opt_append.buf);
	if (opt_upload_pack.len)
		argv_array_push(&args, opt_upload_pack.buf);
	argv_push_force(&args);
	if (opt_tags.len)
		argv_array_push(&args, opt_tags.buf);
	if (opt_prune.len)
		argv_array_push(&args, opt_prune.buf);
	if (opt_recurse_submodules.len)
		argv_array_push(&args, opt_recurse_submodules.buf);
	if (opt_dry_run)
		argv_array_push(&args, "--dry-run");
	if (opt_keep.len)
		argv_array_push(&args, opt_keep.buf);
	if (opt_depth.len)
		argv_array_push(&args, opt_depth.buf);
	if (opt_unshallow.len)
		argv_array_push(&args, opt_unshallow.buf);
	if (opt_update_shallow.len)
		argv_array_push(&args, opt_update_shallow.buf);
	if (opt_refmap.len)
		argv_array_push(&args, opt_refmap.buf);

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
	struct sha1_array merge_heads = SHA1_ARRAY_INIT;

	if (!getenv("_GIT_USE_BUILTIN_PULL")) {
		const char *path = mkpath("%s/git-pull", git_exec_path());

		if (sane_execvp(path, (char**) argv) < 0)
			die_errno("could not exec %s", path);
	}

	argc = parse_options(argc, argv, prefix, pull_options, pull_usage, 0);

	parse_repo_refspecs(argc, argv, &repo, &refspecs);

	if (!opt_ff.len)
		config_get_ff(&opt_ff);

	if (run_fetch(repo, refspecs))
		return 1;

	if (opt_dry_run)
		return 0;

	get_merge_heads(&merge_heads);

	if (!merge_heads.nr)
		die_no_merge_candidates(repo, refspecs);

	return run_merge();
}
