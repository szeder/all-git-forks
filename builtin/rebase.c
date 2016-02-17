#include "cache.h"
#include "parse-options.h"
#include "argv-array.h"
#include "run-command.h"
#include "tree-walk.h"
#include "unpack-trees.h"
#include "diff.h"
#include "commit.h"
#include "revision.h"
#include "submodule.h"
#include "commit.h"
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

/* These are exported as environment variables for git-rebase--*.sh */
static int action_abort;
static int action_continue;
static int action_skip;
static int autosquash;
static int force_rebase;
static int keep_empty;
static int preserve_merges;
static int quiet;
static int rerere_autoupdate;
static int root;
static int verbose;
static const char *exec_cmd;
static const char *head_name;
static const char *onto;
static const char *orig_head;
static struct strbuf revisions = STRBUF_INIT;
static const char *state_basedir;
static const char *strategy;
static const char *strategy_opts;
static const char *switch_to;
static const char *squash_onto;

static int do_merge;
static int edit_todo;
static int interactive_rebase;
static int rebase_type;
static int show_stat;

static char *read_file(const char *name)
{
	struct strbuf sb = STRBUF_INIT;
	if (strbuf_read_file(&sb,
			     git_path("%s/%s", state_basedir, name),
			     0) >= 0)
		return strbuf_detach(&sb, NULL);
	else
		return NULL;
}

static char *read_file_or_die(const char *name)
{
	struct strbuf sb = STRBUF_INIT;
	strbuf_read_file_or_die(&sb,
				git_path("%s/%s", state_basedir, name),
				0);
	return strbuf_detach(&sb, NULL);
}

static void read_bool(const char *name, int *var)
{
	char *buf = read_file(name);
	if (buf) {
		*var = buf[0] && !isspace(buf[0]);
		free(buf);
	}
}

static int read_bool_or_die(const char *name)
{
	char *buf = read_file_or_die(name);
	int ret = buf[0] && !isspace(buf[0]);
	free(buf);
	return ret;
}

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
	head_name = read_file_or_die("head-name");
	onto = read_file_or_die("onto");
	/*
	 * We always write to orig-head, but interactive rebase used
	 * to write to head. Fall back to reading from head to cover
	 * for the case that the user upgraded git with an ongoing
	 * interactive rebase.
	 */
	if ((orig_head = read_file("orig-head")) == NULL)
		orig_head = read_file_or_die("head");
	quiet = read_bool_or_die("quiet");
	read_bool("verbose", &verbose);
	strategy = read_file("strategy");
	strategy_opts = read_file("strategy_opts");
	read_bool("allow_rerere_autoupdate", &rerere_autoupdate);
}

static void push_env_string(struct argv_array *argv,
			    const char *name,
			    const char *value)
{
	struct strbuf sb = STRBUF_INIT;
	strbuf_addf(&sb, "%s=%s", name, value);
	argv_array_push(argv, sb.buf);
	strbuf_release(&sb);
}

static void push_env_bool(struct argv_array *argv,
			  const char *name,
			  int value)
{
	struct strbuf sb = STRBUF_INIT;
	strbuf_addf(&sb, "%s=%s", name, value ? "t" : "");
	argv_array_push(argv, sb.buf);
	strbuf_release(&sb);
}

static int run_specific_rebase()
{
	struct child_process cmd;
	struct argv_array env = ARGV_ARRAY_INIT;
	const char *argv[2];
	int ret;

	if (interactive_rebase == -1) {
		argv_array_push(&env, "GIT_EDITOR=:");
		autosquash = 0;
	}
	push_env_bool(&env, "git_quiet", quiet);
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
	push_env_bool(&env,   "allow_rerere_autoupdate", rerere_autoupdate);
	push_env_bool(&env,   "autosquash",              autosquash);
	push_env_string(&env, "cmd",                     exec_cmd);
	push_env_bool(&env,   "force_rebase",            force_rebase);
	push_env_string(&env, "git_am_opt", ""/* git_am_opt */);
	push_env_string(&env, "head_name",               head_name);
	push_env_bool(&env,   "keep_empty",              keep_empty);
	push_env_string(&env, "onto",                    onto);
	push_env_string(&env, "orig_head",               orig_head);
	push_env_bool(&env,   "preserve_merges",         preserve_merges);
	push_env_bool(&env,   "rebase_root",             root);
	push_env_string(&env, "revisions",               revisions.buf);
	push_env_string(&env, "squash_onto",             squash_onto);
	push_env_string(&env, "state_dir", git_path("%s", state_basedir));
	push_env_string(&env, "strategy", strategy);
	push_env_string(&env, "strategy_opts", strategy_opts);
	push_env_string(&env, "switch_to", switch_to);
	/* upstream */
	push_env_bool(&env, "verbose", verbose);

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
	int ret, error = 0;
	unsigned char sha1[20];
	if (get_sha1("HEAD", sha1))
		die(_("Cannot read HEAD"));

	ret = reset_tree(lookup_commit(sha1)->tree, quiet, 1, &error);
	if (ret)
		return ret;
	read_basic_state();
	return run_specific_rebase();
}

static int do_abort()
{
	int ret, error = 0;
	unsigned char sha1[20];
	const char *rerere[] = { "rerere", "clear", NULL };
	struct strbuf path = STRBUF_INIT;

	ret = run_command_v_opt(rerere, RUN_GIT_CMD);
	if (ret)
		return ret;
	read_basic_state();
	if (!prefixcmp(head_name, "refs/"))
		create_symref("HEAD", head_name, "rebase: aborting");

	if (get_sha1(orig_head, sha1))
		die(_("Cannot read %s"), orig_head);

	ret = reset_tree(lookup_commit(sha1)->tree, quiet, 1, &error);
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
		die(_("The --edit-todo action can only be "
		      "used during interactive rebase."));
	return run_specific_rebase();
}

static void error_on_missing_default_upstream()
{
	unsigned char sha1[20];
	const char *head = resolve_ref_unsafe("HEAD", sha1, 0, NULL);
	if (!head)
		die(_("Failed to resolve HEAD as a valid ref."));
	if (!strcmp(head, "HEAD")) {
		printf(_("You are not currently on a branch. Please specify which\n"
			 "branch you want to rebase against. See git-rebase(1) for details.\n"
			 "\n"
			 "    git rebase <branch>\n"));
		return;
	}

	if (!prefixcmp(head, "refs/heads/"))
		head += strlen("refs/heads/");
	printf(_("There is no tracking information for the current branch.\n"
		 "Please specify which branch you want to rebase against.\n"
		 "See git-rebase(1) for details\n"
		 "\n"
		 "    git rebase <branch>\n"
		 "\n"
		 "If you wish to set tracking information for this branch you can do so with:\n"
		 "\n"
		 "    git branch --set-upstream-to=$remote/<branch> %s\n"),
	       head);
}

static int determine_upstream(int argc, const char **argv,
			      const struct option *options,
			      const char **upstream_name,
			      const char **upstream_arg)
{
	int consumed = 0;
	if (!root) {
		unsigned char sha1[20];
		if (argc) {
			*upstream_name = argv[0];
			argv++;
			argc--;
			consumed++;
		} else {
			*upstream_name = "@{upstream}";
			if (get_sha1(*upstream_name, sha1)) {
				error_on_missing_default_upstream();
				exit(1);
			}
		}
		if (get_sha1(*upstream_name, sha1))
			die(_("Failed to resolve %s."), *upstream_name);
		lookup_commit_or_die(sha1, *upstream_name);
		*upstream_arg = *upstream_name;
		return consumed;
	}

	if (!onto) {
		unsigned char sha1[20];
		struct strbuf sb = STRBUF_INIT;
		if (commit_tree(&sb, EMPTY_TREE_SHA1_BIN, NULL, sha1, NULL, NULL))
			die(_("failed to create empty-tree commit"));
		onto = xstrdup(sha1_to_hex(sha1));
		squash_onto = onto;
	}
	if (argc > 1)
		usage_with_options(builtin_rebase_usage,
				   options);
	*upstream_arg = "--root";
	return consumed;
}

static void require_clean_work_tree()
{
	struct rev_info rev;
	unsigned char sha1[20];
	int err = 0;

	if (get_sha1("HEAD", sha1))
		die("HEAD is not a valid ref");

	if (read_cache() < 0)
		die(_("unable to read index file"));
	refresh_cache(REFRESH_QUIET | REFRESH_IGNORE_SUBMODULES);

	init_revisions(&rev, NULL);
	DIFF_OPT_SET(&rev.diffopt, QUICK);
	DIFF_OPT_SET(&rev.diffopt, OVERRIDE_SUBMODULE_CONFIG);
	handle_ignore_submodules_arg(&rev.diffopt, "all");
	diff_setup_done(&rev.diffopt);
	run_diff_files(&rev, 0);
	if (DIFF_OPT_TST(&rev.diffopt, HAS_CHANGES)) {
		fputs(_("Cannot rebase: You have unstaged changes.\n"), stderr);
		err = 1;
	}

	init_revisions(&rev, NULL);
	DIFF_OPT_SET(&rev.diffopt, QUICK);
	DIFF_OPT_SET(&rev.diffopt, OVERRIDE_SUBMODULE_CONFIG);
	handle_ignore_submodules_arg(&rev.diffopt, "all");
	diff_setup_done(&rev.diffopt);
	add_head_to_pending(&rev);
	run_diff_index(&rev, 1);
	if (DIFF_OPT_TST(&rev.diffopt, HAS_CHANGES)) {
		const char *msg = err ?
			_("Additionally, your index contains uncommitted changes.\n") :
			_("Cannot rebase: Your index contains uncommitted changes.\n");
		fputs(msg, stderr);
		err = 1;
	}

	if (err) {
		fputs(_("Please commit or stash them.\n"), stderr);
		exit(1);
	}
}

static void run_pre_rebase_hook(int argc, const char **argv,
				const char *upstream_arg)
{
	/*
	if test -z "$ok_to_skip_pre_rebase" &&
	   test -x "$GIT_DIR/hooks/pre-rebase"
	then
		"$GIT_DIR/hooks/pre-rebase" ${1+"$@"} ||
		die "$(gettext "The pre-rebase hook refused to rebase.")"
	fi
	 */
}

static int do_rebase(int argc, const char **argv,
		     const struct option *options)
{
	const char *upstream_name = NULL;
	const char *upstream_arg = NULL;
	const char *branch_name;
	const char *onto_name;
	unsigned char onto_sha1[20];

	int n;
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

	n = determine_upstream(argc, argv, options,
			       &upstream_name, &upstream_arg);
	argc += n;
	argv += n;

	if (root)
		upstream_arg = "--root";

	/* Make sure the branch to rebase onto is valid. */
	onto_name = onto ? onto : upstream_name;
	if (strstr(onto, "...")) {
		/*
case "$onto_name" in
*...*)
	if	left=${onto_name%...*} right=${onto_name#*...} &&
		onto=$(git merge-base --all ${left:-HEAD} ${right:-HEAD})
	then
		case "$onto" in
		?*"$LF"?*)
			die "$(eval_gettext "\$onto_name: there are more than one merge bases")"
			;;
		'')
			die "$(eval_gettext "\$onto_name: there is no merge base")"
			;;
		esac
	else
		die "$(eval_gettext "\$onto_name: there is no merge base")"
	fi
	;;
		*/
	} else {
		if (get_sha1(onto_name, onto_sha1) || !lookup_commit(onto_sha1))
			die(_("Does not point to a valid commit: %s"), onto_name);
	}

	/*
	 * If the branch to rebase is given, that is the branch we
	 * will rebase
	 *
	 * $branch_name -- branch being rebased, or HEAD (already
	 * detached)
	 *
	 * $orig_head -- commit object name of tip of the branch
	 * before rebasing
	 *
	 * $head_name -- refs/heads/<that-branch> or "detached HEAD"
	 */
	switch (argc) {
	case 1:
		/* Is it "rebase other $branchname" or "rebase other $commit"? */
		branch_name = argv[0];
		switch_to = argv[0];
		/*
	if git show-ref --verify --quiet -- "refs/heads/$1" &&
	   orig_head=$(git rev-parse -q --verify "refs/heads/$1")
	then
		head_name="refs/heads/$1"
	elif orig_head=$(git rev-parse -q --verify "$1")
	then
		head_name="detached HEAD"
	else
		die "$(eval_gettext "fatal: no such branch: \$branch_name")"
	fi
	 */
		break;

	case 0:
		/*
	# Do not need to switch branches, we are already on it.
	if branch_name=`git symbolic-ref -q HEAD`
	then
		head_name=$branch_name
		branch_name=`expr "z$branch_name" : 'zrefs/heads/\(.*\)'`
	else
		head_name="detached HEAD"
		branch_name=HEAD ;# detached
	fi
	orig_head=$(git rev-parse --verify "${branch_name}^0") || exit
		 */
		break;
	default:
		die("BUG: unexpected number of arguments left to parse");
	}

	require_clean_work_tree();

	/*
	 * Now we are rebasing commits $upstream..$orig_head (or with
	 * --root, everything leading up to $orig_head) on top of
	 * $onto
	 */

	/*
	 * Check if we are already based on $onto with linear history,
	 * but this should be done only when upstream and onto are the
	 * same and if this is not an interactive rebase.
	 */

	/*
mb=$(git merge-base "$onto" "$orig_head")
if test "$type" != interactive && test "$upstream" = "$onto" &&
	test "$mb" = "$onto" &&
	# linear history?
	! (git rev-list --parents "$onto".."$orig_head" | sane_grep " .* ") > /dev/null
then
	if test -z "$force_rebase"
	then
		# Lazily switch to the target branch if needed...
		test -z "$switch_to" || git checkout "$switch_to" --
		say "$(eval_gettext "Current branch \$branch_name is up to date.")"
		exit 0
	else
		say "$(eval_gettext "Current branch \$branch_name is up to date, rebase forced.")"
	fi
fi
	*/

	/* If a hook exists, give it a chance to interrupt */
	run_pre_rebase_hook(argc, argv, upstream_arg);

	/*
if test -n "$diffstat"
then
	if test -n "$verbose"
	then
		echo "$(eval_gettext "Changes from \$mb to \$onto:")"
	fi
	# We want color (if set), but no pager
	GIT_PAGER='' git diff --stat --summary "$mb" "$onto"
fi
	*/

	if (rebase_type == REBASE_INTERACTIVE)
		return run_specific_rebase();

	/* Detach HEAD and reset the tree */
	/*
say "$(gettext "First, rewinding head to replay your work on top of it...")"
git checkout -q "$onto^0" || die "could not detach HEAD"
git update-ref ORIG_HEAD $orig_head
	*/

	/*
	 * If the $onto is a proper descendant of the tip of the
	 * branch, then we just fast-forwarded.
	 */
	if (!hashcmp(mb, orig_head)) {
		printf(_("Fast-forwarded %s to %s."), branch_name, onto_name);
		move_to_original_branch();
		return 0;
	}

	if (root)
		strbuf_addf(&revisions, "%s..%s", onto, orig_head);
	else
		strbuf_addf(&revisions, "%s..%s", upstream, orig_head);

	return run_specific_rebase();
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
	const char *whitespace_opt = NULL;
	int committer_date_is_author_date = 0;
	int context_opt = 0;
	int ignore_date = 0;
	int ignore_whitespace = 0;
	int no_ff = 0;
	int pre_rebase = 1;
	struct option options[] = {
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
	gitmodules_config();

	argc = parse_options(argc, argv, prefix, options,
			     builtin_rebase_usage, 0);

	action_nr = action_continue + action_skip + action_abort + edit_todo;
	if ((action_nr != 1 && action_nr != 0) || argc > 2)
		usage_with_options(builtin_rebase_usage, options);

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

	return do_rebase(argc, argv, options);
}
