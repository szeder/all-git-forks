/*
 * Builtin "git rebase"
 */
#include "cache.h"
#include "builtin.h"
#include "parse-options.h"
#include "remote.h"
#include "refs.h"
#include "revision.h"
#include "lockfile.h"
#include "diff.h"
#include "run-command.h"
#include "rebase-common.h"
#include "rebase-am.h"
#include "rebase-interactive.h"
#include "unpack-trees.h"

enum rebase_type {
	REBASE_TYPE_NONE = 0,
	REBASE_TYPE_AM,
	REBASE_TYPE_INTERACTIVE
};

enum rebase_subcommand {
	REBASE_RUN = 0,
	REBASE_CONTINUE,
	REBASE_ABORT,
	REBASE_SKIP,
	REBASE_EDIT_TODO
};

static enum rebase_type rebase_in_progress(void)
{
	if (rebase_am_in_progress(NULL))
		return REBASE_TYPE_AM;
	else if (rebase_interactive_in_progress(NULL))
		return REBASE_TYPE_INTERACTIVE;
	else
		return REBASE_TYPE_NONE;
}

static const char *rebase_dir(enum rebase_type type)
{
	switch (type) {
	case REBASE_TYPE_AM:
		return git_path_rebase_am_dir();
	case REBASE_TYPE_INTERACTIVE:
		return git_path_rebase_interactive_dir();
	default:
		die("BUG: invalid rebase_type %d", type);
	}
}

/**
 * Used by get_curr_branch_upstream_name() as a for_each_remote() callback to
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

const char *get_curr_branch_upstream_name(void)
{
	const char *upstream_name;
	struct branch *curr_branch;

	curr_branch = branch_get("HEAD");
	if (!curr_branch) {
		fprintf_ln(stderr, _("You are not currently on a branch."));
		fprintf_ln(stderr, _("Please specify which branch you want to rebase against."));
		fprintf_ln(stderr, _("See git-rebase(1) for details."));
		fprintf(stderr, "\n");
		fprintf_ln(stderr, "    git rebase <branch>");
		fprintf(stderr, "\n");
		exit(1);
	}

	upstream_name = branch_get_upstream(curr_branch, NULL);
	if (!upstream_name) {
		const char *remote_name = NULL;

		if (for_each_remote(get_only_remote, &remote_name) || !remote_name)
			remote_name = "<remote>";

		fprintf_ln(stderr, _("There is no tracking information for the current branch."));
		fprintf_ln(stderr, _("Please specify which branch you want to rebase against."));
		fprintf_ln(stderr, _("See git-rebase(1) for details."));
		fprintf(stderr, "\n");
		fprintf_ln(stderr, "    git rebase <branch>");
		fprintf(stderr, "\n");
		fprintf_ln(stderr, _("If you wish to set tracking information for this branch you can do so with:"));
		fprintf(stderr, "\n");
		fprintf_ln(stderr, _("If you wish to set tracking information for this branch you can do so with:\n"
		"\n"
		"    git branch --set-upstream-to=%s/<branch> %s\n"),
		remote_name, curr_branch->name);
		exit(1);
	}

	return upstream_name;
}

/**
 * Given the --onto <name>, return the onto hash
 */
static void get_onto_oid(const char *_onto_name, struct object_id *onto)
{
	char *onto_name = xstrdup(_onto_name);
	struct commit *onto_commit;
	char *dotdot;

	dotdot = strstr(onto_name, "...");
	if (dotdot) {
		const char *left = onto_name;
		const char *right = dotdot + 3;
		struct commit *left_commit, *right_commit;
		struct commit_list *merge_bases;

		*dotdot = 0;
		if (!*left)
			left = "HEAD";
		if (!*right)
			right = "HEAD";

		/* git merge-base --all $left $right */
		left_commit = lookup_commit_reference_by_name(left);
		right_commit = lookup_commit_reference_by_name(right);
		if (!left_commit || !right_commit)
			die(_("%s: there is no merge base"), _onto_name);

		merge_bases = get_merge_bases(left_commit, right_commit);
		if (merge_bases && merge_bases->next)
			die(_("%s: there are more than one merge bases"), _onto_name);
		else if (!merge_bases)
			die(_("%s: there is no merge base"), _onto_name);

		onto_commit = merge_bases->item;
		free_commit_list(merge_bases);
	} else {
		onto_commit = lookup_commit_reference_by_name(onto_name);
		if (!onto_commit)
			die(_("invalid upstream %s"), onto_name);
	}

	free(onto_name);
	oidcpy(onto, &onto_commit->object.oid);
}

static void rebase_continue(enum rebase_type type)
{
	unsigned char head[GIT_SHA1_RAWSZ];

	/* git rev-parse --verify HEAD */
	if (get_sha1("HEAD", head))
		die(_("Cannot read HEAD"));

	/* git update-index --ignore-submodules --refresh (no --quiet) */
	if (refresh_and_write_cache(REFRESH_IGNORE_SUBMODULES) < 0)
		die(_("failed to refresh index"));

	/* git diff-files --quiet --ignore-submodules || You must edit all
	 * merge conflicts */
	if (unmerged_cache() || cache_has_unstaged_changes(1))
		die(_("You must edit all merge conflicts and then mark them as resolved using git add"));

	/* read_basic_state and run_specific_rebase */
	switch (type) {
	case REBASE_TYPE_AM: {
		struct rebase_am state;

		rebase_am_init(&state, rebase_dir(REBASE_TYPE_AM));
		if (rebase_am_load(&state) < 0)
			exit(1);
		rebase_am_continue(&state);
		rebase_am_release(&state);
	} break;
	case REBASE_TYPE_INTERACTIVE: {
		struct rebase_interactive state;

		rebase_interactive_init(&state, rebase_dir(REBASE_TYPE_INTERACTIVE));
		if (rebase_interactive_load(&state) < 0)
			exit(1);
		rebase_interactive_continue(&state);
		rebase_interactive_release(&state);
	} break;
	default:
		die("BUG: invalid rebase_type %d", type);
	}
}

static void rebase_skip(const struct rebase_options *opts, enum rebase_type type)
{
	struct object_id head;

	if (get_oid("HEAD", &head))
		die(_("Cannot read HEAD"));

	/* git reset --hard HEAD || exit $? */
	if (reset_hard(&head) < 0)
		die(_("failed to reset index"));

	/* read_basic_state and run_specific_rebase */
	switch (type) {
	case REBASE_TYPE_AM: {
		struct rebase_am state;

		rebase_am_init(&state, rebase_dir(REBASE_TYPE_AM));
		if (rebase_am_load(&state) < 0)
			exit(1);
		rebase_am_skip(&state);
		rebase_am_release(&state);
	} break;
	case REBASE_TYPE_INTERACTIVE: {
		struct rebase_interactive state;

		rebase_interactive_init(&state, rebase_dir(REBASE_TYPE_INTERACTIVE));
		if (rebase_interactive_load(&state) < 0)
			exit(1);
		rebase_interactive_skip(&state);
		rebase_interactive_release(&state);
	} break;
	default:
		die("BUG: invalid rebase_type %d", type);
	}
}

static void rebase_abort(enum rebase_type type)
{
	struct rebase_options opts;
	const char *dir = rebase_dir(type);

	rebase_options_init(&opts);

	/* git rerere clear */

	/* read_basic_state */
	if (rebase_options_load(&opts, dir) < 0)
		exit(1);

	/* move back to orig_refname */
	if (opts.orig_refname && starts_with(opts.orig_refname, "refs/")) {
		if (create_symref("HEAD", opts.orig_refname, "rebase: aborting"))
			die(_("Could not move back to %s"), opts.orig_refname);
	}

	/* git reset --hard $orig_head */
	reset_hard(&opts.orig_head);

	/* finish_rebase */
	rebase_common_finish(&opts, dir);

	rebase_options_release(&opts);
}

static int parse_opt_verbose(const struct option *opt, const char *arg, int unset)
{
	struct rebase_options *rebase_opts = opt->value;
	int *diffstat = (int *)opt->defval;
	rebase_opts->verbose = 1;
	rebase_opts->quiet = 0;
	*diffstat = 1;
	return 0;
}

static int parse_opt_quiet(const struct option *opt, const char *arg, int unset)
{
	struct rebase_options *rebase_opts = opt->value;
	int *diffstat = (int *)opt->defval;
	rebase_opts->quiet = 1;
	rebase_opts->verbose = 0;
	*diffstat = 0;
	return 0;
}

static int parse_opt_strategy(const struct option *opt, const char *arg, int unset)
{
	char **value = opt->value;
	free(*value);
	*value = unset ? NULL : xstrdup(arg);
	return 0;
}

static int parse_opt_strategy_option(const struct option *opt, const char *arg, int unset)
{
	struct argv_array *value = opt->value;
	argv_array_pushf(value, "--%s", arg);
	return 0;
}

#define OPT_STRING_NONEG(s, l, v, a, h) { OPTION_STRING, (s), (l), (v), (a), (h), PARSE_OPT_NONEG }
#define OPT_BOOL_NONEG(s, l, v, h) { OPTION_SET_INT, (s), (l), (v), NULL, (h), PARSE_OPT_NOARG | PARSE_OPT_NONEG, NULL, 1 }

static int git_rebase_config(const char *k, const char *v, void *cb)
{
	int status;

	status = git_gpg_config(k, v, NULL);
	if (status)
		return status;

	return git_default_config(k, v, NULL);
}

int cmd_rebase(int argc, const char **argv, const char *prefix)
{
	struct rebase_options rebase_opts;
	enum rebase_type type;
	int fork_point = 0;
	int preserve_merges = 0;
	int diffstat = 0;
	int do_merge = 0;
	int interactive = 0;
	int interactive_implied = 0;
	struct string_list cmd = STRING_LIST_INIT_NODUP;
	int keep_empty = 0;
	int verify = 1;
	int autosquash = 0;
	struct argv_array git_am_opt = ARGV_ARRAY_INIT;
	enum rebase_subcommand subcommand = REBASE_RUN;
	struct object_id squash_onto;

	const char *onto_name = NULL;
	const char *branch_name;

	const char * const usage[] = {
		N_("git rebase [<options>]"),
		NULL
	};

	struct option options[] = {
		OPT_GROUP(N_("Available options are")),
		{ OPTION_CALLBACK, 'v', "verbose", &rebase_opts, NULL,
		  N_("display a diffstat of what changed upstream"),
		  PARSE_OPT_NOARG | PARSE_OPT_NONEG, parse_opt_verbose, (intptr_t)&diffstat },
		{ OPTION_CALLBACK, 'q', "quiet", &rebase_opts, NULL,
		  N_("be quiet. implies --no-stat"),
		  PARSE_OPT_NOARG | PARSE_OPT_NONEG, parse_opt_quiet, (intptr_t)&diffstat },
		OPT_BOOL(0, "autostash", &rebase_opts.autostash,
			N_("automatically stash/stash pop before and after")),
		OPT_BOOL(0, "fork-point", &fork_point,
			N_("use 'merge-base --fork-point' to refine upstream")),
		OPT_STRING_NONEG(0, "onto", &onto_name, NULL,
			N_("rebase onto given branch instead of upstream")),
		OPT_BOOL_NONEG('p', "preserve-merges", &preserve_merges,
			N_("try to recreate merges instead of ignoring them")),
		{ OPTION_CALLBACK, 's', "strategy", &rebase_opts.strategy, NULL,
		  N_("use the given merge strategy"),
		  PARSE_OPT_NONEG, parse_opt_strategy },
		OPT_BOOL_NONEG(0, "no-ff", &rebase_opts.force,
			N_("cherry-pick all commits, even if unchanged")),
		OPT_BOOL_NONEG('m', "merge", &do_merge,
			N_("use merging strategies to rebase")),
		OPT_BOOL_NONEG('i', "interactive", &interactive,
			N_("let the user edit the list of commits to rebase")),
		OPT_STRING_LIST('x', "exec", &cmd, NULL,
			N_("add exec lines after each commit of the editable list")),
		OPT_BOOL('k', "keep-empty", &keep_empty,
			N_("preserve empty commits during rebase")),
		OPT_BOOL_NONEG('f', "force-rebase", &rebase_opts.force,
			N_("force rebase even if branch is up to date")),
		{ OPTION_CALLBACK, 'X', "strategy-option", &rebase_opts.strategy_opts, NULL,
		  N_("pass the argument through to the merge strategy"),
		  PARSE_OPT_NONEG, parse_opt_strategy_option },
		{ OPTION_SET_INT, 0, "stat", &diffstat, NULL,
		  N_("display a diffstat of what changed upstream"),
		  PARSE_OPT_NOARG | PARSE_OPT_NONEG, NULL, 1 },
		{ OPTION_SET_INT, 'n', "no-stat", &diffstat, NULL,
		  N_("do not show diffstat of what changed upstream"),
		  PARSE_OPT_NOARG | PARSE_OPT_NONEG, NULL, 0 },
		OPT_BOOL(0, "verify", &verify,
			N_("allow pre-rebase hook to run")),
		OPT_PASSTHRU(0, "rerere-autoupdate", &rebase_opts.allow_rerere_autoupdate, NULL,
			N_("allow rerere to update index with resolved conflicts"),
			PARSE_OPT_NOARG),
		OPT_BOOL_NONEG(0, "root", &rebase_opts.root,
			N_("rebase all reachable commits up to the root(s)")),
		OPT_BOOL(0, "autosquash", &autosquash,
			N_("move commits that begin with squash!/fixup! under -i")),
		OPT_PASSTHRU_ARGV(0, "committer-date-is-author-date",
				&git_am_opt, NULL,
				N_("passed to 'git am'"),
				PARSE_OPT_NOARG | PARSE_OPT_NONEG),
		OPT_PASSTHRU_ARGV(0, "ignore-date",
				&git_am_opt, NULL,
				N_("passed to 'git am'"),
				PARSE_OPT_NOARG | PARSE_OPT_NONEG),
		OPT_PASSTHRU_ARGV(0, "whitespace", &git_am_opt, NULL,
				N_("passed to 'git apply'"),
				PARSE_OPT_NONEG),
		OPT_PASSTHRU_ARGV(0, "ignore-whitespace", &git_am_opt, NULL,
				N_("passed to 'git apply'"),
				PARSE_OPT_NOARG | PARSE_OPT_NONEG),
		OPT_PASSTHRU_ARGV('C', NULL, &git_am_opt, NULL,
				N_("passed to 'git apply'"),
				PARSE_OPT_NONEG),
		OPT_PASSTHRU('S', "gpg-sign", &rebase_opts.gpg_sign_opt, NULL,
				N_("GPG-sign commits"),
				PARSE_OPT_OPTARG),

		OPT_GROUP(N_("Actions:")),
		OPT_CMDMODE(0, "continue", &subcommand,
			N_("continue"),
			REBASE_CONTINUE),
		OPT_CMDMODE(0, "abort", &subcommand,
			N_("abort and check out the original branch"), REBASE_ABORT),
		OPT_CMDMODE(0, "skip", &subcommand,
			N_("skip current patch and continue"),
			REBASE_SKIP),
		OPT_CMDMODE(0, "edit-todo", &subcommand,
			N_("edit the todo list during an interactive rebase"),
			REBASE_EDIT_TODO),
		OPT_END()
	};
	int i;

	git_config(git_rebase_config, NULL);

	rebase_options_init(&rebase_opts);
	oidclr(&squash_onto);
	rebase_opts.resolvemsg = _("\nWhen you have resolved this problem, run \"git rebase --continue\".\n"
			"If you prefer to skip this patch, run \"git rebase --skip\" instead.\n"
			"To check out the original branch and stop rebasing, run \"git rebase --abort\".");

	type = rebase_in_progress();

	argc = parse_options(argc, argv, prefix, options, usage, 0);

	if (read_index_preload(&the_index, NULL) < 0)
		die(_("failed to read the index"));

	if (rebase_opts.strategy || rebase_opts.strategy_opts.argc)
		do_merge = 1;
	if (rebase_opts.strategy_opts.argc && !rebase_opts.strategy)
		rebase_opts.strategy = xstrdup("recursive");

	for (i = 0; i < git_am_opt.argc; i++) {
		const char *arg = git_am_opt.argv[i];
		if (!strcmp(arg, "--committer-date-is-author-date") ||
		    !strcmp(arg, "--ignore-date") ||
		    !strcmp(arg, "--whitespace=fix") ||
		    !strcmp(arg, "--whitespace=strip"))
			rebase_opts.force = 1;
	}

	if (cmd.nr && !interactive) {
		fprintf_ln(stderr, _("The --exec option must be used with the --interactive option"));
		exit(128);
	}

	/* Implied interactive rebase */
	if ((preserve_merges || rebase_opts.root) && !interactive) {
		interactive = 1;
		interactive_implied = 1;
	}

	if (subcommand != REBASE_RUN) {
		if (type == REBASE_TYPE_NONE)
			die(_("No rebase in progress?"));
		/* Only interactive rebase uses detailed reflog messages */
	}

	switch (subcommand) {
	case REBASE_RUN:
		break; /* do nothing */
	case REBASE_CONTINUE:
		rebase_continue(type);
		goto finish;
	case REBASE_SKIP:
		rebase_skip(&rebase_opts, type);
		goto finish;
	case REBASE_ABORT:
		rebase_abort(type);
		goto finish;
	case REBASE_EDIT_TODO:
		break;
	default:
		die("BUG: invalid subcommand %d", subcommand);
	}

	/*
	 * Parse command-line arguments:
	 *    rebase [<options>] [<upstream_name> [<branch_name>]]
	 * or rebase [<options>] --root [<branch_name>]
	 */
	if (!rebase_opts.root) {
		const char *upstream_name;

		if (argc > 2)
			usage_with_options(usage, options);

		/* Get upstream if not provided */
		if (!argc) {
			upstream_name = get_curr_branch_upstream_name();
		} else {
			upstream_name = argv[0];
			argv++, argc--;
			if (!strcmp(upstream_name, "-"))
				upstream_name = "@{-1}";
		}

		if (get_sha1_commit(upstream_name, rebase_opts.upstream.hash))
			die(_("invalid upstream %s"), upstream_name);

		if (!onto_name)
			onto_name = upstream_name;
	} else {
		if (argc > 1)
			usage_with_options(usage, options);

		if (!onto_name) {
			/* Create a commit with an empty tree and use that as
			 * onto */
			if (commit_tree("", 0, EMPTY_TREE_SHA1_BIN, NULL, squash_onto.hash, NULL, NULL))
				die("commit_tree() failed");
			onto_name = xstrdup(oid_to_hex(&squash_onto));
			oidcpy(&rebase_opts.upstream, &squash_onto);
		}
	}

	/* Parse --onto <onto_name> */
	get_onto_oid(onto_name, &rebase_opts.onto);
	rebase_opts.onto_name = xstrdup(onto_name);

	/* Parse branch_name */
	branch_name = argv[0];
	if (branch_name) {
		/* Is branch_name a branch or commit? */
		char *ref_name = xstrfmt("refs/heads/%s", branch_name);
		struct object_id orig_head_id;

		if (!read_ref(ref_name, orig_head_id.hash)) {
			rebase_opts.orig_refname = ref_name;
			if (get_sha1_commit(ref_name, rebase_opts.orig_head.hash))
				die("get_sha1_commit failed");
		} else if (!get_sha1_commit(branch_name, rebase_opts.orig_head.hash)) {
			rebase_opts.orig_refname = NULL;
			free(ref_name);
		} else {
			die(_("no such branch: %s"), branch_name);
		}
	} else {
		/* Do not need to switch branches, we are already on it */
		struct branch *curr_branch = branch_get("HEAD");

		if (curr_branch)
			rebase_opts.orig_refname = xstrdup(curr_branch->refname);
		else
			rebase_opts.orig_refname = NULL;

		if (get_sha1_commit("HEAD", rebase_opts.orig_head.hash))
			die(_("Failed to resolve '%s' as a valid revision."), "HEAD");
	}

	if (interactive) {
		struct rebase_interactive state;

		rebase_interactive_init(&state, rebase_dir(REBASE_TYPE_INTERACTIVE));
		rebase_options_swap(&state.opts, &rebase_opts);
		if (interactive_implied)
			setenv("GIT_EDITOR", ":", 1);
		if (preserve_merges)
			state.preserve_merges = 1;
		if (autosquash)
			state.autosquash = 1;
		oidcpy(&state.squash_onto, &squash_onto);
		rebase_interactive_run(&state, &cmd);
		rebase_interactive_release(&state);
	} else if (do_merge) {
		die("TODO: merge rebase");
	} else {
		struct rebase_am state;

		rebase_am_init(&state, rebase_dir(REBASE_TYPE_AM));
		rebase_options_swap(&state.opts, &rebase_opts);
		rebase_am_run(&state, git_am_opt.argv);
		rebase_am_release(&state);
	}

finish:
	string_list_clear(&cmd, 0);
	argv_array_clear(&git_am_opt);
	rebase_options_release(&rebase_opts);
	return 0;
}
