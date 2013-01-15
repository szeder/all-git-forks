#include "cache.h"
#include "parse-options.h"
#include "argv-array.h"
#include "run-command.h"
#include "dir.h"

#define REBASE_AM		1
#define REBASE_MERGE		2
#define REBASE_INTERACTIVE	3

static const char * const builtin_rebase_usage[] = {
	N_("git rebase [-i] [options] [--exec <cmd>] [--onto <newbase>] [<upstream>] [<branch>]"),
	N_("git rebase [-i] [options] [--exec <cmd>] [--onto <newbase>] --root [<branch>]"),
	N_("git rebase --continue | --abort | --skip | --edit-todo"),
	NULL
};

static int verbose, quiet, preserve_merges, no_ff, do_merge, interactive_rebase;
static int keep_empty, force_rebase, show_stat, pre_rebase = 1, rerere_autoupdate;
static int root, autosquash, committer_date_is_author_date;
static int ignore_date, context_opt, ignore_whitespace;
static int action_continue, action_abort, action_skip, edit_todo;
static const char *onto, *strategy, *strategy_opts, *exec_cmd, *whitespace_opt;
static int rebase_type;
static const char *state_basedir;
static const char *head_name, *orig_head;

static struct option builtin_rebase_options[] = {
	OPT__VERBOSE(&verbose,
		N_("display a diffstat of what changed upstream")),
	OPT__QUIET(&quiet,
		N_("be quiet. implies --no-stat")),
	OPT_STRING(0, "onto", &onto, "ref",
		N_("rebase onto given branch instead of upstream")),
	OPT_BOOL('p', "preserve-merges", &preserve_merges,
		N_("try to recreate merges instead of ignoring them")),
	OPT_STRING('s', "stragegy", &strategy, "strategy",
		N_("use the given merge strategy")),
	{ OPTION_SET_INT, 0, "no-ff", &no_ff, NULL,
		N_("cherry-pick all commits, even if unchanged"),
		PARSE_OPT_NOARG | PARSE_OPT_NONEG, NULL, 1 },
	OPT_BOOL('m', "merge", &do_merge,
		N_("use merging strategies to rebase")),
	OPT_BOOL('i', "interactive", &interactive_rebase,
		N_("let the user edit the list of commits to rebase")),
	OPT_STRING('x', "exec", &exec_cmd, "command",
		N_("add exec lines after each commit of the editable list")),
	OPT_BOOL('k', "keep-empty", &keep_empty,
		N_("preserve empty commits during rebase")),
	OPT_BOOL('f', "force-rebase", &force_rebase,
		N_("force rebase even if branch is up to date")),
	OPT_STRING('X', "strategy-options", &strategy_opts, "options",
		N_("pass the argument through to the merge strategy")),
	{ OPTION_SET_INT, 0, "stat", &show_stat, NULL,
		N_("display a diffstat of what changed upstream"),
		PARSE_OPT_NOARG | PARSE_OPT_NONEG, NULL, 1 },
	OPT_SET_INT('n', "no-stat", &show_stat,
		N_("do not show diffstat of what changed upstream"), 0),
	OPT_BOOL(0, "verify", &pre_rebase,
		N_("allow pre-rebase hook to run")),
	OPT_BOOL(0, "rerere-autoupdate", &rerere_autoupdate,
		N_("allow rerere to update index with resolved conflicts")),
	OPT_BOOL(0, "root", &root,
		N_("rebase all reachable commits up to the root(s)")),
	OPT_BOOL(0, "autosquash", &autosquash,
		N_("move commits that begin with squash!/fixup! under -i")),
	OPT_BOOL(0, "committer-date-is-author-date", &committer_date_is_author_date,
		N_("lie about committer date")),
	OPT_BOOL(0, "ignore-date", &ignore_date,
		N_("use current timestamp for author date")),
	OPT_STRING(0, "whitespace", &whitespace_opt, "mode",
		N_("detect new or modified lines that have whitespace errors")),
	OPT_INTEGER('C', NULL, &context_opt,
		N_("ensure at least <n> lines of context match by 'git apply'")),
	OPT_BOOL(0, "ignore-whitespace", &ignore_whitespace,
		N_("ignore changes in whitespace when finding context")),

	OPT_GROUP("Actions"),
	OPT_BOOL(0, "continue", &action_continue,
		N_("continue")),
	OPT_BOOL(0, "abort", &action_abort,
		N_("abort and check out the original branch")),
	OPT_BOOL(0, "skip", &action_skip,
		N_("skip current patch and continue")),
	OPT_BOOL(0, "edit-todo", &edit_todo,
		N_("edit the todo list during an interactive rebase")),
	OPT_END()
};

/*
 * Note:
 *
 * After git-rebase--*.sh are integrated, we should probably adopt
 * git-config format and store everything in one file instead of so
 * many like this. state_dir will be something different to avoid
 * misuse by old rebase versions. This code will stay for a few major
 * releases before being phased out.
 */
static void read_basic_state()
{
	struct strbuf sb = STRBUF_INIT;

	strbuf_read_file_or_die(&sb, git_path("%s/head-name", state_basedir), 0);
	head_name = strbuf_detach(&sb, NULL);

	strbuf_read_file_or_die(&sb, git_path("%s/onto", state_basedir), 0);
	onto = strbuf_detach(&sb, NULL);

	/*
	 * We always write to orig-head, but interactive rebase used to write to
	 * head. Fall back to reading from head to cover for the case that the
	 * user upgraded git with an ongoing interactive rebase.
	 */
	if (strbuf_read_file(&sb, git_path("%s/orig-head", state_basedir), 0) < 0)
		strbuf_read_file_or_die(&sb, git_path("%s/head", state_basedir), 0);
	orig_head = strbuf_detach(&sb, NULL);

	strbuf_read_file_or_die(&sb, git_path("%s/quiet", state_basedir), 0);
	quiet = sb.len && !isspace(sb.buf[0]);
	strbuf_setlen(&sb, 0);

	if (strbuf_read_file(&sb, git_path("%s/verbose", state_basedir), 0) >= 0) {
		verbose = sb.len && !isspace(sb.buf[0]);
		strbuf_setlen(&sb, 0);
	}

	if (strbuf_read_file(&sb, git_path("%s/strategy", state_basedir), 0) >= 0)
		strategy = strbuf_detach(&sb, NULL);

	if (strbuf_read_file(&sb, git_path("%s/strategy_opts", state_basedir), 0) >= 0)
		strategy_opts = strbuf_detach(&sb, NULL);

	if (strbuf_read_file(&sb, git_path("%s/allow_rerere_autoupdate", state_basedir), 0) >= 0)
		rerere_autoupdate = sb.len && !isspace(sb.buf[0]);

	strbuf_release(&sb);
}

static int run_specific_rebase()
{
	struct child_process cmd;
	struct argv_array env = ARGV_ARRAY_INIT;
	struct strbuf sb = STRBUF_INIT;
	const char *argv[2];
	int ret;

	/*
	 * Export variables required by git-rebase--*.sh
	 */
	if (interactive_rebase == -1) {
		argv_array_push(&env, "GIT_EDITOR=:");
		autosquash = 0;
	}
	argv_array_push(&env, quiet ? "git_quiet=t" : "git_quiet=");
	if (action_continue)
		argv_array_push(&env, "action=continue");
	else if (action_skip)
		argv_array_push(&env, "action=skip");
	else if (action_abort)
		argv_array_push(&env, "action=abort");
	else if (edit_todo)
		argv_array_push(&env, "action=edit-todo");
	else
		argv_array_push(&env, "action=");
	argv_array_push(&env, rerere_autoupdate ?
			"allow_rerere_autoupdate=t" :
			"allow_rerere_autoupdate=");
	argv_array_push(&env, autosquash ? "autosquash=t" : "autosquash=");
	strbuf_addf(&sb, "cmd=%s", exec_cmd);
	argv_array_push(&env, sb.buf);
	argv_array_push(&env, force_rebase ?
			"force_rebase=t" : "force_rebase=");
	strbuf_setlen(&sb, 0);
	strbuf_addf(&sb, "git_am_opt=%s", "" /* git_am_opt */);
	argv_array_push(&env, sb.buf);
	strbuf_setlen(&sb, 0);
	strbuf_addf(&sb, "head_name=%s", head_name);
	argv_array_push(&env, sb.buf);
	argv_array_push(&env, keep_empty ? "keep_empty=t" : "keep_empty=");
	strbuf_setlen(&sb, 0);
	strbuf_addf(&sb, "onto=%s", onto);
	argv_array_push(&env, sb.buf);
	strbuf_setlen(&sb, 0);
	strbuf_addf(&sb, "orig_head=%s", orig_head);
	argv_array_push(&env, sb.buf);
	argv_array_push(&env, preserve_merges ?
			"preserve_merges=t" : "preserve_merges=");
	argv_array_push(&env, root ? "rebase_root=t" : "rebase_root=");
	/* revisions */
	/* squash_onto */
	strbuf_setlen(&sb, 0);
	strbuf_addf(&sb, "state_dir=%s/%s", get_git_dir(), state_basedir);
	argv_array_push(&env, sb.buf);
	strbuf_setlen(&sb, 0);
	strbuf_addf(&sb, "strategy=%s", strategy);
	argv_array_push(&env, sb.buf);
	strbuf_setlen(&sb, 0);
	strbuf_addf(&sb, "strategy_opts=%s", strategy_opts);
	argv_array_push(&env, sb.buf);
	/* switch_to */
	/* upstream */
	argv_array_push(&env, verbose ? "verbose=t" : "verbose=");

	switch (rebase_type) {
	case REBASE_INTERACTIVE: argv[0] = "rebase--interactive"; break;
	case REBASE_AM:		 argv[0] = "rebase--am";	  break;
	case REBASE_MERGE:	 argv[0] = "rebase--merge";	  break;
	default:
		die("BUG: unsupported rebase type %d", rebase_type);
	}
	argv[1] = NULL;

	memset(&cmd, 0, sizeof(cmd));
	cmd.in  = 0;
	cmd.out = 1;
	cmd.err = 2;
	cmd.git_cmd = 1;
	cmd.env = env.argv;
	cmd.argv = argv;
	ret = run_command(&cmd);
	if (ret)
		die_errno("failed to run %s", argv[0]);

	argv_array_clear(&env);
	return ret;
}

static int do_continue()
{
	unsigned char sha1[20];
	if (get_sha1("HEAD", sha1))
		die(_("Cannot read HEAD"));
	if (read_cache_unmerged()) {
		printf(_("You must edit all merge conflicts and then\n"
			 "mark them as resolved using 'git add'\n"));
		return 1;
	}
	read_basic_state();
	return run_specific_rebase();
}

static int do_skip()
{
	const char *reset[] = { "reset", "--hard", "HEAD", NULL };
	int ret = run_command_v_opt(reset, RUN_GIT_CMD); /* output */
	if (ret)
		return ret;
	read_basic_state();
	return run_specific_rebase();
}

static int do_abort()
{
	const char *rerere[] = { "rerere", "clear", NULL };
	const char *reset[] = { "reset", "--hard", NULL, NULL };
	struct strbuf path = STRBUF_INIT;
	int ret = run_command_v_opt(rerere, RUN_GIT_CMD);
	if (ret)
		return ret;
	read_basic_state();
	if (!prefixcmp(head_name, "refs/"))
		; /* git symbolic-ref -m "rebase: aborting" HEAD $head_name */
	reset[2] = orig_head;
	ret = run_command_v_opt(reset, RUN_GIT_CMD); /* output */
	if (ret)
		return ret;
	strbuf_addstr(&path, git_path("%s", state_basedir));
	remove_dir_recursively(&path, 0);
	strbuf_release(&path);
	return 0;
}

static int do_edit_todo()
{
	if (rebase_type != REBASE_INTERACTIVE)
		die(_("The --edit-todo action can only be used during interactive rebase."));
	return run_specific_rebase();
}

static int do_rebase()
{
	if (root && !onto && !interactive_rebase)
		interactive_rebase = -1; /* implied */

	if (interactive_rebase) {
		rebase_type = REBASE_INTERACTIVE;
		state_basedir = "rebase-merge";
	} else if (do_merge) {
		rebase_type = REBASE_MERGE;
		state_basedir = "rebase-merge";
	} else {
		rebase_type = REBASE_AM;
		state_basedir = "rebase-apply";
	}

	if (root) {
	} else {
	}
}

static int git_rebase_config(const char *name, const char *value, void *data)
{
	if (!strcmp(name, "rebase.stat")) {
		show_stat = git_config_bool(name, value);
		return 0;
	} else if (!strcmp(name, "rebase.autosquash")) {
		autosquash = git_config_bool(name, value);
		return 0;
	}
	return git_default_config(name, value, data);
}

int cmd_rebase(int argc, const char **argv, const char *prefix)
{
	struct stat st;
	int action_nr, in_progress;

	if (!stat(git_path("rebase-apply"), &st) && S_ISDIR(st.st_mode)) {
		if (!access(git_path("rebase-apply/applying"), F_OK))
			die(_("It looks like git-am is in progress. Cannot rebase."));
		rebase_type = REBASE_AM;
		state_basedir = "rebase-apply";
		in_progress = 1;
	} else if (!stat(git_path("rebase-merge"), &st) &&
		   S_ISDIR(st.st_mode)) {
		if (!access(git_path("rebase-merge/interactive"), F_OK)) {
			rebase_type = REBASE_INTERACTIVE;
			interactive_rebase = 1; /* explicit */
		} else
			rebase_type = REBASE_MERGE;
		state_basedir = "rebase-merge";
		in_progress = 1;
	} else
		in_progress = 0;

	git_config(git_rebase_config, NULL);

	argc = parse_options(argc, argv, prefix, builtin_rebase_options,
			     builtin_rebase_usage, 0);

	action_nr = action_continue + action_skip + action_abort + edit_todo;
	if ((action_nr != 1 && action_nr != 0) || argc > 2)
		usage_with_options(builtin_rebase_usage,
				   builtin_rebase_options);

	if (verbose && quiet)
		die(_("--quiet and --verbose are incompatible"));
	else if (verbose)
		show_stat = 1;
	else if (quiet)
		show_stat = 0;

	if (preserve_merges && !interactive_rebase)
		interactive_rebase = -1; /* implied */

	if (strategy_opts) {
		if (!strategy)
			strategy = "recursive";
		/* validate strategy options */
	}

	if (strategy)
		do_merge = 1;

	if (!strcmp(whitespace_opt, "fix") ||
	    !strcmp(whitespace_opt, "strip") ||
	    committer_date_is_author_date ||
	    no_ff)
		force_rebase = 1;

	if (exec_cmd && interactive_rebase != 1)
		die(_("The --exec option must be used with the --interactive option"));

	if (action_nr) {
		if (!in_progress)
			die(_("No rebase in progress?"));
		if (rebase_type == REBASE_INTERACTIVE)
			; /* GIT_REFLOG_ACTION = "rebase -i ($action)" */

		if (action_continue)
			return do_continue();
		else if (action_skip)
			return do_skip();
		else if (action_abort)
			return do_abort();
		else if (edit_todo)
			return do_edit_todo();
		else
			die("BUG: how do you get here?");
	}

	if (in_progress)
		die(_("It seems that there is already a %s directory, and\n"
		      "I wonder if you are in the middle of another rebase. If that is the\n"
		      "case, please try\n"
		      "\tgit rebase (--continue | --abort | --skip)\n"
		      "If that is not the case, please\n"
		      "\trm -rf \"%s/%s\""
		      "and run me again. I am stopping in case you still have something\n"
		      "valuable there."),
		    state_basedir,
		    get_git_dir(), state_basedir);

	return do_rebase();
}
