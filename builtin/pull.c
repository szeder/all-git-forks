/*
 * Copyright (c) 2015 Stephen Robin <stephen.robin@gmail.com>
 *
 * Based on git-pull.sh by Junio C Hamano
 */

#include "builtin.h"
#include "parse-options.h"
#include "submodule.h"
#include "dir.h"
#include "run-command.h"
#include "argv-array.h"
#include "fmt-merge-msg.h"
#include "refs.h"
#include "revision.h"
#include "remote.h"

enum ff_type {
	FF_NOT_SET = -1,
	FF_NO,
	FF_ALLOW,
	FF_ONLY
};

enum pull_mode {
	PULL_NOT_SET = -1,
	PULL_MERGE,
	PULL_REBASE,
	PULL_PRESERVE_MERGES_REBASE,
	PULL_INTERACTIVE_REBASE
};

static const char * const builtin_pull_usage[] = {
	N_("git pull [<options>] [<fetch-options>] [<repository> [<refspec>...]]"),
	NULL
};

static enum pull_mode pull_mode_from_args = PULL_NOT_SET;
static enum pull_mode pull_mode_from_branch_config = PULL_NOT_SET;
static enum pull_mode pull_mode_from_config = PULL_NOT_SET;

static enum ff_type ff_type_from_args = FF_NOT_SET;
static enum ff_type ff_type_from_config = FF_NOT_SET;

/*
 * These are passed to the underlying fetch / merge / rebase commands but not
 * used to make decisions within pull itself.
 *  1  command line includes --option
 *  0  command line includes --no-option
 *  -1 neither is on command line
 */
static int progress = -1;
static int show_diffstat = -1;
static int shortlog_len = -1;
static int squash = -1;
static int option_commit = -1;
static int option_edit = -1;
static int verify_signatures = -1;

static int dryrun;
static int verbosity;
static int recurse_submodules = RECURSE_SUBMODULES_DEFAULT;

static const char **use_strategies;
static size_t use_strategies_nr, use_strategies_alloc;

static const char **xopts;
static size_t xopts_nr, xopts_alloc;

static const char *option_gpg_sign;

static const char *curr_branch, *curr_branch_short;

static int parse_pull_mode(const char *name, const char* arg,
	enum pull_mode *option_rebase)
{
	int arg_as_bool = git_config_maybe_bool(name, arg);

	if (arg_as_bool == 0) {
		*option_rebase = PULL_MERGE;
		return 0;
	}

	if (arg_as_bool == 1) {
		*option_rebase = PULL_REBASE;
		return 0;
	}

	if (!strcmp(arg, "interactive")) {
		*option_rebase = PULL_INTERACTIVE_REBASE;
		return 0;
	}

	if (!strcmp(arg, "i")) {
		*option_rebase = PULL_INTERACTIVE_REBASE;
		return 0;
	}

	if (!strcmp(arg, "preserve")) {
		*option_rebase = PULL_PRESERVE_MERGES_REBASE;
		return 0;
	}

	error(_("Invalid value for %s, should be 'true', 'false', 'interactive' or 'preserve'."), name);
	return -1;
}

static int parse_ff_type(const char *name, const char* arg,
	enum ff_type *fast_forward)
{
	int arg_as_bool = git_config_maybe_bool(name, arg);

	if (arg_as_bool == 0) {
		*fast_forward = FF_NO;
		return 0;
	}

	if (arg_as_bool == 1) {
		*fast_forward = FF_ALLOW;
		return 0;
	}

	if (!strcmp(arg, "only")) {
		*fast_forward = FF_ONLY;
		return 0;
	}

	error(_("Invalid value for %s, should be 'true', 'false', or 'only'."), name);
	return -1;
}

static int git_pull_config(const char *key, const char *value, void *cb)
{
	if (curr_branch_short &&
		starts_with(key, "branch.") &&
		starts_with(key + 7, curr_branch_short) &&
		!strcmp(key + 7 + strlen(curr_branch_short), ".rebase"))
		return parse_pull_mode(key, value, &pull_mode_from_branch_config);

	if (!strcmp(key, "pull.rebase"))
		return parse_pull_mode(key, value, &pull_mode_from_config);

	if (!strcmp(key, "pull.ff"))
		return parse_ff_type(key, value, &ff_type_from_config);

	return fmt_merge_msg_config(key, value, cb);
}

static int option_parse_n(const struct option *opt,
	const char *arg, int unset)
{
	show_diffstat = unset;
	return 0;
}

static int option_parse_recurse_submodules(const struct option *opt,
	const char *arg, int unset)
{
	if (unset)
		recurse_submodules = RECURSE_SUBMODULES_OFF;
	else if (arg)
		recurse_submodules = parse_fetch_recurse_submodules_arg(opt->long_name, arg);
	else
		recurse_submodules = RECURSE_SUBMODULES_ON;

	return 0;
}

static int option_parse_rebase(const struct option *opt,
	const char *arg, int unset)
{
	if (unset) {
		pull_mode_from_args = PULL_MERGE;
		return 0;
	}

	if (!arg) {
		pull_mode_from_args = PULL_REBASE;
		return 0;
	}

	if (!parse_pull_mode(opt->long_name, arg, &pull_mode_from_args))
		return 0;

	return -1;
}

static int option_parse_strategy(const struct option *opt,
	const char *arg, int unset)
{
	if (unset)
		return 0;

	ALLOC_GROW(use_strategies, use_strategies_nr + 1, use_strategies_alloc);
	use_strategies[use_strategies_nr++] = xstrdup(arg);
	return 0;
}

static int option_parse_x(const struct option *opt,
	const char *arg, int unset)
{
	if (unset)
		return 0;

	ALLOC_GROW(xopts, xopts_nr + 1, xopts_alloc);
	xopts[xopts_nr++] = xstrdup(arg);
	return 0;
}

static struct option builtin_pull_options[] = {
	{ OPTION_CALLBACK, 0, "rebase", NULL, N_("true|false|interactive|preserve"),
		N_("incorporate changes by rebasing rather than merging"),
		PARSE_OPT_OPTARG, option_parse_rebase },
	OPT_BOOL(0, "progress", &progress,
		N_("force progress reporting")),
	OPT__VERBOSITY(&verbosity),
	{ OPTION_CALLBACK, 0, "recurse-submodules", NULL, N_("on-demand"),
		N_("control recursive fetching of submodules"),
	PARSE_OPT_OPTARG, option_parse_recurse_submodules },
	/* We can't use OPT__DRY_RUN as it uses 'n' as the short form, which we use as the short form of stat. */
	OPT_BOOL(0, "dry-run", &dryrun, N_("dry run")),
	{ OPTION_CALLBACK, 'n', NULL, NULL, NULL,
		N_("do not show a diffstat at the end of the merge"),
		PARSE_OPT_NOARG, option_parse_n },
	OPT_BOOL(0, "stat", &show_diffstat,
		N_("show a diffstat at the end of the merge")),
	OPT_HIDDEN_BOOL(0, "summary", &show_diffstat, N_("(synonym to --stat)")),
	{ OPTION_INTEGER, 0, "log", &shortlog_len, N_("n"),
		N_("add (at most <n>) entries from shortlog to merge commit message"),
		PARSE_OPT_OPTARG, NULL, DEFAULT_MERGE_LOG_LEN },
	OPT_BOOL(0, "squash", &squash,
		N_("create a single commit instead of doing a merge")),
	OPT_BOOL(0, "commit", &option_commit,
		N_("perform a commit if the merge succeeds (default)")),
	OPT_BOOL('e', "edit", &option_edit,
		N_("edit message before committing")),
	OPT_SET_INT(0, "ff", &ff_type_from_args,
		N_("allow fast-forward (default)"), FF_ALLOW),
	{ OPTION_SET_INT, 0, "ff-only", &ff_type_from_args, NULL,
		N_("abort if fast-forward is not possible"),
		PARSE_OPT_NOARG | PARSE_OPT_NONEG, NULL, FF_ONLY },
	OPT_BOOL(0, "verify-signatures", &verify_signatures,
		N_("verify that the named commit has a valid GPG signature")),
	OPT_CALLBACK('s', "strategy", &use_strategies, N_("strategy"),
		N_("merge strategy to use"), option_parse_strategy),
	OPT_CALLBACK('X', "strategy-option", &xopts, N_("option=value"),
		N_("option for selected merge strategy"), option_parse_x),
	{ OPTION_STRING, 'S', "gpg-sign", &option_gpg_sign, N_("key-id"),
		N_("GPG sign commit"), PARSE_OPT_OPTARG, NULL, (intptr_t) "" },
	OPT_END()
};

static enum pull_mode get_pull_mode()
{
	/* use --[no-]rebase[=preserve] from the command line, if specified. */
	if (pull_mode_from_args != PULL_NOT_SET)
		return pull_mode_from_args;

	/* otherwise use branch.<name>.rebase from config, if set. */
	if (pull_mode_from_branch_config != PULL_NOT_SET)
		return pull_mode_from_branch_config;

	/* otherwise use pull.rebase from config, if set. */
	if (pull_mode_from_config != PULL_NOT_SET)
		return pull_mode_from_config;

	/* use merge by default. */
	return PULL_MERGE;
}

static enum ff_type get_ff_type()
{
	if (ff_type_from_args != FF_NOT_SET)
		return ff_type_from_args;

	if (ff_type_from_config != FF_NOT_SET)
		return ff_type_from_config;

	return FF_ALLOW;
}

static char *get_merge_msg()
{
	const char *inpath = NULL;
	FILE *in;
	struct strbuf input = STRBUF_INIT, output = STRBUF_INIT;
	struct fmt_merge_msg_opts opts;

	inpath = git_path("FETCH_HEAD");
	in = fopen(inpath, "r");
	if (!in)
		die_errno("cannot open '%s'", inpath);

	if (strbuf_read(&input, fileno(in), 0) < 0)
		die_errno("could not read '%s'", inpath);

	fclose(in);

	memset(&opts, 0, sizeof(opts));
	opts.add_title = 1;
	opts.credit_people = 1;
	opts.shortlog_len = shortlog_len;

	fmt_merge_msg(&input, &output, &opts);

	strbuf_release(&input);

	return strbuf_detach(&output, NULL);
}

static const struct string_list get_merge_head()
{
	/*
	 * Read FETCH_HEAD line by line
	 * ... exclude lines containing \tnot-for-merge\t
	 * ... exclude everything after the first tab in remaining lines
	 * ... result is a list of sha1s to be merged.
	 */

	const char *filename;
	FILE *fp;
	struct strbuf line = STRBUF_INIT;
	struct string_list merge_head = STRING_LIST_INIT_DUP;
	char *ptr;

	filename = git_path("FETCH_HEAD");
	fp = fopen(filename, "r");
	if (!fp)
		die_errno(_("could not open '%s' for reading"), filename);

	while (strbuf_getline(&line, fp, '\n') != EOF) {

		ptr = strstr(line.buf, "\tnot-for-merge\t");
		if (!ptr) {
			ptr = strchr(line.buf, '\t');
			if (ptr) {
				strbuf_setlen(&line, ptr - line.buf);
				string_list_append(&merge_head, line.buf);
			}
		}

		strbuf_reset(&line);
	}

	fclose(fp);
	strbuf_release(&line);

	return merge_head;
}

static int run_fetch(const int additional_argc, const char **additional_argv)
{
	int v, q, idx, result;
	struct argv_array argv = ARGV_ARRAY_INIT;

	argv_array_push(&argv, "fetch");

	for (v = verbosity; v > 0; v--)
		argv_array_push(&argv, "-v");

	for (q = verbosity; q < 0; q++)
		argv_array_push(&argv, "-q");

	if (progress == 1)
		argv_array_push(&argv, "--progress");
	else if (progress == 0)
		argv_array_push(&argv, "--no-progress");

	if (dryrun)
		argv_array_push(&argv, "--dry-run");

	if (recurse_submodules == RECURSE_SUBMODULES_ON)
		argv_array_push(&argv, "--recurse-submodules");
	else if (recurse_submodules == RECURSE_SUBMODULES_ON_DEMAND)
		argv_array_push(&argv, "--recurse-submodules=on-demand");
	else if (recurse_submodules == RECURSE_SUBMODULES_OFF)
		argv_array_push(&argv, "--no-recurse-submodules");

	argv_array_push(&argv, "--update-head-ok");

	for (idx = 0; idx < additional_argc; idx++)
		argv_array_push(&argv, additional_argv[idx]);

	result = run_command_v_opt(argv.argv, RUN_GIT_CMD);

	argv_array_clear(&argv);

	return result;
}

static int run_merge(const struct string_list merge_head)
{
	int v, q, idx, result;
	struct argv_array argv = ARGV_ARRAY_INIT;
	const char *merge_msg;
	enum ff_type fast_forward_type;

	merge_msg = get_merge_msg();
	fast_forward_type = get_ff_type();


	argv_array_push(&argv, "merge");

	if (show_diffstat == 1)
		argv_array_push(&argv, "--stat");
	else if (show_diffstat == 0)
		argv_array_push(&argv, "--no-stat");

	if (option_commit == 1)
		argv_array_push(&argv, "--commit");
	else if (option_commit == 0)
		argv_array_push(&argv, "--no-commit");

	if (verify_signatures == 1)
		argv_array_push(&argv, "--verify-signatures");
	else if (verify_signatures == 0)
		argv_array_push(&argv, "--no-verify-signatures");

	if (option_edit == 1)
		argv_array_push(&argv, "--edit");
	else if (option_edit == 0)
		argv_array_push(&argv, "--no-edit");

	if (squash == 1)
		argv_array_push(&argv, "--squash");
	else if (squash == 0)
		argv_array_push(&argv, "--no-squash");

	if (fast_forward_type == FF_ALLOW)
		argv_array_push(&argv, "--ff");
	else if (fast_forward_type == FF_NO)
		argv_array_push(&argv, "--no-ff");
	else if (fast_forward_type == FF_ONLY)
		argv_array_push(&argv, "--ff-only");

	if (shortlog_len >= 0)
		argv_array_pushf(&argv, "--log=%d", shortlog_len);

	for (idx = 0; idx < use_strategies_nr; idx++)
		argv_array_pushf(&argv, "--strategy=%s", use_strategies[idx]);

	for (idx = 0; idx < xopts_nr; idx++)
		argv_array_pushf(&argv, "-X%s", xopts[idx]);

	for (v = verbosity; v > 0; v--)
		argv_array_push(&argv, "-v");

	for (q = verbosity; q < 0; q++)
		argv_array_push(&argv, "-q");

	if (progress == 1)
		argv_array_push(&argv, "--progress");
	else if (progress == 0)
		argv_array_push(&argv, "--no-progress");

	if (option_gpg_sign)
		argv_array_pushf(&argv, "-S%s", option_gpg_sign);

	argv_array_pushf(&argv, "\"%s\"", merge_msg);

	argv_array_push(&argv, "HEAD");

	for (idx = 0; idx < merge_head.nr; idx++)
		argv_array_push(&argv, merge_head.items[idx].string);

	result = run_command_v_opt(argv.argv, RUN_GIT_CMD);

	argv_array_clear(&argv);

	return result;
}

static const char *run_show_branch(const char *merge_head,
	const char *fork_point_for_rebase)
{
	/*
	 * TODO Should be able to do the same thing without needing to fork another
	 * git instance. It's just a simple search of the graph after all.
	 */

	int len;
	struct child_process cp;
	struct strbuf buffer = STRBUF_INIT;
	struct argv_array argv = ARGV_ARRAY_INIT;

	argv_array_push(&argv, "show-branch");
	argv_array_push(&argv, "--merge-base");

	argv_array_push(&argv, curr_branch);
	argv_array_push(&argv, merge_head);
	argv_array_push(&argv, fork_point_for_rebase);

	memset(&cp, 0, sizeof(cp));
	cp.argv = argv.argv;
	cp.out = -1;
	cp.git_cmd = 1;

	if (start_command(&cp))
		die(_("could not run git show-branch."));
	len = strbuf_read(&buffer, cp.out, 1024);
	close(cp.out);

	if (finish_command(&cp) || len < 0)
		die(_("show-branch failed"));
	else if (!len)
		return NULL;

	return strbuf_detach(&buffer, NULL);
}

static int run_rebase(const struct string_list merge_head, const char *fork_point,
	const enum pull_mode pull_mode)
{
	int v, q, idx, result;
	struct argv_array argv = ARGV_ARRAY_INIT;

	if (merge_head.nr > 1)
		die(_("Cannot rebase onto multiple branches"));

	if (fork_point) {
		const char *show_branch_result = run_show_branch(merge_head.items[0].string, fork_point);
		if (!strcmp(fork_point, show_branch_result))
			fork_point = NULL;
	}

	argv_array_push(&argv, "rebase");

	if (show_diffstat == 1)
		argv_array_push(&argv, "--stat");
	else if (show_diffstat == 0)
		argv_array_push(&argv, "--no-stat");

	for (idx = 0; idx < use_strategies_nr; idx++)
		argv_array_pushf(&argv, "--strategy=%s", use_strategies[idx]);

	for (idx = 0; idx < xopts_nr; idx++)
		argv_array_pushf(&argv, "-X%s", xopts[idx]);

	if (pull_mode == PULL_PRESERVE_MERGES_REBASE)
		argv_array_push(&argv, "--preserve-merges");
	else if (pull_mode == PULL_INTERACTIVE_REBASE)
		argv_array_push(&argv, "-i");

	for (v = verbosity; v > 0; v--)
		argv_array_push(&argv, "-v");

	for (q = verbosity; q < 0; q++)
		argv_array_push(&argv, "-q");

	if (option_gpg_sign)
		argv_array_pushf(&argv, "-S%s", option_gpg_sign);

	argv_array_push(&argv, "--onto");
	argv_array_push(&argv, merge_head.items[0].string);

	if (fork_point)
		argv_array_push(&argv, fork_point);
	else
		argv_array_push(&argv, merge_head.items[0].string);

	result = run_command_v_opt(argv.argv, RUN_GIT_CMD);

	argv_array_clear(&argv);

	return result;
}

static int get_remote_name(struct remote *remote, void *priv)
{
	struct string_list *list = priv;
	string_list_append(list, remote->name);
	return 0;
}

static void error_on_no_merge_candidates(enum pull_mode mode,
	int argc, const char **argv)
{
	const char *op_type, *op_prep;
	int idx;
	struct branch *branch;

	/*
	 * TODO Existing bug in git-pull.sh, add another patch to the series to fix:
	 * This function fails to take into account any arguments to be passed to
	 * git fetch other than the remote and the refs.
	 *
	 * Set up a pair of test repos as follows:
	 *   mkdir repo
	 *   cd repo
	 *   git init
	 *   echo test > test
	 *   git add test
	 *   git commit -m "a commit"
	 *   cd ..
	 *   git clone repo cloned
	 *   cd cloned
	 *   git remote add nondefaultremote "../repo"
	 *
	 * Now compare the output of:
	 *   git pull nondefaultremote
	 *   git pull -p nondefaultremote
	 *
	 * The messages should be identical but aren't, the second is incorrect.
	 */

	for (idx = 0; idx < argc; idx++) {
		if (!strcmp("-t", argv[idx]) || starts_with(argv[idx], "--t"))
			die(_("It doesn't make sense to pull all tags; you probably meant:\n"
			"git fetch --tags"));
	}

	branch = branch_get(curr_branch_short);

	if (mode == PULL_MERGE) {
		op_type = "merge";
		/*
		 * TRANSLATORS: This is the preposition associated with the merge
		 * operation. In English it is used as "specify the branch you want to
		 * merge _with_"
		 */
		op_prep = _("with");
	} else {
		op_type = "rebase";
		/*
		* TRANSLATORS: This is the preposition associated with the rebase
		* operation. In English it is used as "specify the branch you want to
		* rebase _against_"
		*/
		op_prep = _("against");
	}

	if (argc > 1) {
		if (mode == PULL_MERGE)
			die(_("There are no candidates for merging\n"
			"among the refs that you just fetched.\n"
			"Generally this means that you provided a wildcard refspec which had no\n"
			"matches on the remote end."));
		else
			die(_("There is no candidate for rebasing against\n"
			"among the refs that you just fetched.\n"
			"Generally this means that you provided a wildcard refspec which had no\n"
			"matches on the remote end.\n"));
	} else if (argc > 0 && branch && branch->remote_name &&
		strcmp(argv[0], branch->remote_name)) {
		die(_("You asked to pull from the remote '%s', but did not specify\n"
			"a branch. Because this is not the default configured remote\n"
			"for your current branch, you must specify a branch on the command line."), argv[0]);
	} else if (!curr_branch_short) {
		/*
		 * TRANSLATORS: first parameter is the operation (merge or rebase),
		 * second is the preposition (with or against in English).
		 */
		die(_("You are not currently on a branch. Please specify which\n"
			"branch you want to %s %s. See git help pull for details.\n\n"
			"  git pull <remote> <branch>"), op_type, op_prep);
	} else if (!branch || !branch->merge || !branch->merge[0] ||
		!branch->merge[0]->dst || !branch->remote_name) {
		/* If there's only one remote, use that in the suggestion */
		struct string_list list = STRING_LIST_INIT_NODUP;
		char *remote_name;

		for_each_remote(get_remote_name, &list);

		if (list.nr == 1)
			remote_name = list.items[0].string;
		else
			remote_name = "<remote>";
		/*
		* TRANSLATORS: first parameter is the operation (merge or rebase),
		* second is the preposition (with or against in English).
		*/
		die(_("There is no tracking information for the current branch.\n"
			"Please specify which branch you want to %s %s.\n"
			"See git help pull for details\n\n"
			"  git pull <remote> <branch>\n\n"
			"If you wish to set tracking information for this branch you can do so with:\n\n"
			"  git branch --set-upstream-to=%s/<branch> %s"),
			op_type, op_prep, remote_name, curr_branch_short);
	} else {
		const char *upstream_short = strncmp(branch->merge[0]->dst, "refs/heads/", 11)
			? branch->merge[0]->dst : branch->merge[0]->dst + 11;
		/*
		* TRANSLATORS: first parameter is the operation (merge or rebase),
		* second is the preposition (with or against in English).
		*/
		die(_("Your configuration specifies to %s %s the ref '%s'\n"
			"from the remote, but no such ref was fetched."),
			op_type, op_prep, upstream_short);
	}
}

static const char *get_remote_merge_branch(int argc, const char **argv)
{
	/*
	 * TODO Existing bug in git-pull.sh, add another patch to the series to fix:
	 * This function fails to take into account any arguments to be passed to git
	 * fetch other than the remote and the refs.
	 *
	 * See also error_on_no_merge_candidates, it has the same problem.
	 */

	/*
	 * TODO Existing bug in git-pull.sh, add another patch to the series to fix:
	 * This function doesn't always take into account mapping of remote to local
	 * branch names.
	 */

	if (argc <= 1) {
		struct remote *my_remote;
		struct branch *branch;

		my_remote = (argc == 1) ? remote_get(argv[0]) : remote_get(NULL);
		if (!my_remote || !my_remote->name)
			return NULL;

		branch = branch_get(curr_branch_short);
		if (!branch || !branch->merge || !branch->merge[0] ||
			!branch->merge[0]->dst || !branch->remote_name)
			return NULL;

		if (strcmp(branch->remote_name, my_remote->name))
			return NULL;

		return branch->merge[0]->dst;

	} else {
		/*
		 * TODO Code here is ugly but should do the same thing as
		 * git-parse-remote.sh. I haven't fully tested it as I want to rewrite
		 * the whole function in a subsequent patch anyway.
		 */

		static const char **refs_to_parse = NULL;
		int refs_to_parse_nr = argc - 1;
		int i;
		struct refspec *parsed_refspec;
		char *remote_ref;

		refs_to_parse = xcalloc(argc, sizeof(const char *));
		for (i = 0; i < refs_to_parse_nr; i++)
			refs_to_parse[i] = argv[i + 1];
		refs_to_parse[i] = NULL;

		parsed_refspec = parse_fetch_refspec(refs_to_parse_nr, refs_to_parse);

		remote_ref = parsed_refspec->src;

		if (!remote_ref)
			remote_ref = "HEAD";
		else if (starts_with(remote_ref, "heads/"))
			remote_ref += strlen("heads/");
		else if (starts_with(remote_ref, "refs/heads/"))
			remote_ref += strlen("refs/heads/");
		else if (starts_with(remote_ref, "refs/"))
			remote_ref = NULL;
		else if (starts_with(remote_ref, "tags/"))
			remote_ref = NULL;
		else if (starts_with(remote_ref, "remotes/"))
			remote_ref = NULL;

		if (!remote_ref)
			return NULL;

		if (!strcmp(argv[0], ".")) {
			char *full_ref = xmalloc(strlen(remote_ref) + 12);
			strcpy(full_ref, "refs/heads/");
			strcat(full_ref, remote_ref);
			return full_ref;
		} else {
			char *full_ref = xmalloc(strlen(remote_ref) +strlen(argv[0]) + 15);
			strcpy(full_ref, "refs/remotes/");
			strcat(full_ref, argv[0]);
			strcat(full_ref, "/");
			strcat(full_ref, remote_ref);
			return full_ref;
		}

	}
}

static char *find_fork_point_for_rebase(int argc, const char** argv, unsigned char sha1_orig_head[20])
{
	const char *remote_ref;
	const unsigned char *fork_point_sha1;

	remote_ref = get_remote_merge_branch(argc, argv);
	if (!remote_ref)
		return NULL;

	fork_point_sha1 = get_fork_point(remote_ref, sha1_orig_head);
	if (is_null_sha1(fork_point_sha1))
		return NULL;

	return sha1_to_hex(fork_point_sha1);
}

static int fast_forward_unborn_branch(const struct string_list merge_head)
{
	/*
	* Pulling into an unborn branch.
	* We claim the index is based on an empty tree and try to fast-forward
	* to merge-head. This ensures we will not lose index / worktree
	* changes that the user already made on the unborn branch.
	*/
	unsigned char sha1[20];

	if (merge_head.nr > 1)
		die(_("Cannot merge multiple branches into empty head"));

	if (get_sha1_hex(merge_head.items[0].string, sha1))
		die(_("Unable to find '%s'. FETCH_HEAD may be corrupt"), merge_head.items[0].string);

	if (checkout_fast_forward(EMPTY_TREE_SHA1_BIN, sha1, 0))
		return 1;

	if (update_ref("initial pull", "HEAD", sha1, NULL, 0, UPDATE_REFS_DIE_ON_ERR))
		return 1;

	return 0;
}

static int check_for_unstaged_changes()
{
	struct rev_info rev;
	int result;

	/* Check for changes in the working tree */
	init_revisions(&rev, NULL);
	setup_revisions(0, NULL, &rev, NULL);
	DIFF_OPT_SET(&rev.diffopt, IGNORE_SUBMODULES);
	DIFF_OPT_SET(&rev.diffopt, QUICK);
	DIFF_OPT_SET(&rev.diffopt, HAS_CHANGES);
	DIFF_OPT_SET(&rev.diffopt, EXIT_WITH_STATUS);
	result = run_diff_files(&rev, 0);

	return diff_result_code(&rev.diffopt, result);
}

static int check_for_uncommitted_changes()
{
	struct rev_info rev;
	struct setup_revision_opt opt;
	int result;

	/* Check for changes in the index */
	init_revisions(&rev, NULL);
	memset(&opt, 0, sizeof(opt));
	opt.def = "HEAD";
	setup_revisions(0, NULL, &rev, &opt);
	DIFF_OPT_SET(&rev.diffopt, IGNORE_SUBMODULES);
	DIFF_OPT_SET(&rev.diffopt, QUICK);
	DIFF_OPT_SET(&rev.diffopt, HAS_CHANGES);
	DIFF_OPT_SET(&rev.diffopt, EXIT_WITH_STATUS);
	result = run_diff_index(&rev, 1);

	return diff_result_code(&rev.diffopt, result);
}

static void check_state_before_starting(const enum pull_mode mode, const int unborn)
{
	/*
	 * TODO This function is as close to git-pull.sh as possible. We should be
	 * able to tidy it up and improve it now it's in c (e.g. fail early if
	 * cherry-pick in progress), perhaps using wt_status stuff.
	 */

	if (read_cache_unmerged())
		die_resolve_conflict("pull");

	if (file_exists(git_path("MERGE_HEAD")))
		die_merge_in_progress();

	if (mode != PULL_MERGE) {

		/*
		 * TODO Existing bug in git-pull.sh, add another patch to the series to fix:
		 * We'll die unnecessarily on the next line if files are added to the
		 * index then removed again, leaving the index empty, e.g.
		 *
		 *    mkdir temp
		 *    cd temp
		 *    git init
		 *    echo test > test
		 *    git add test
		 *    git reset test
		 *    rm test
		 *    git pull --rebase ../another-repo
		 * => "updating an unborn branch with changes added to the index"
		 */
		if (unborn && file_exists(get_index_file()))
			die(_("Updating an unborn branch with changes added to the index"));

		if (!unborn) {
			int unstaged_changes = 0;
			int uncommited_changes = 0;

			if (read_cache_preload(NULL) < 0)
				die(_("Corrupt index file"));

			refresh_cache(REFRESH_QUIET | REFRESH_IGNORE_SUBMODULES);

			unstaged_changes = check_for_unstaged_changes();
			uncommited_changes = check_for_uncommitted_changes();

			if (unstaged_changes && uncommited_changes)
				die(_("Cannot pull with rebase: You have unstaged changes and your index contains uncommitted changes./n"
					"Please commit or stash them."));

			if (unstaged_changes)
				die(_("Cannot pull with rebase: You have unstaged changes./n"
					"Please commit or stash them."));

			if (uncommited_changes)
				die(_("Cannot pull with rebase: Your index contains uncommitted changes./n"
					"Please commit or stash them."));
		}
	}
}

static void set_reflog_message(int argc, const char **argv)
{
	int idx;
	struct strbuf reflog_message = STRBUF_INIT;

	for (idx = 0; idx < argc; idx++) {
		strbuf_addstr(&reflog_message, argv[idx]);
		strbuf_addch(&reflog_message, ' ');
	}

	strbuf_trim(&reflog_message);

	setenv("GIT_REFLOG_ACTION", reflog_message.buf, 0);

	strbuf_release(&reflog_message);
}

int cmd_pull(int argc, const char **argv, const char *prefix)
{
	unsigned char sha1_orig_head[20], sha1_curr_head[20];
	enum pull_mode mode = PULL_NOT_SET;
	char *fork_point_for_rebase = NULL;

	set_reflog_message(argc, argv);

	curr_branch = resolve_refdup("HEAD", 0, sha1_orig_head, NULL);
	if (curr_branch && starts_with(curr_branch, "refs/heads/"))
		curr_branch_short = curr_branch + 11;

	git_config(git_pull_config, NULL);

	argc = parse_options(argc, argv, prefix, builtin_pull_options,
		builtin_pull_usage, PARSE_OPT_KEEP_UNKNOWN);

	mode = get_pull_mode();

	if (shortlog_len < 0)
		shortlog_len = (merge_log_config > 0) ? merge_log_config : 0;

	check_state_before_starting(mode, is_null_sha1(sha1_orig_head));

	if (mode != PULL_MERGE && !is_null_sha1(sha1_orig_head))
		fork_point_for_rebase = find_fork_point_for_rebase(argc, argv, sha1_orig_head);

	if (run_fetch(argc, argv))
		return 1;

	if (dryrun)
		return 0;

	get_sha1("HEAD", sha1_curr_head);

	if (!is_null_sha1(sha1_orig_head) &&
		hashcmp(sha1_orig_head, sha1_curr_head)) {
		/*
		 * The fetch involved updating the current branch.
		 * The working tree and the index file are still based on the
		 * orig_head commit, but we are merging into curr_head.
		 * First update the working tree to match curr_head.
		 */

		/* TRANSLATORS: %s is a SHA1 identifying a commit. */
		warning(_("fetch updated the current branch head.\n"
			"fast-forwarding your working tree from\n"
			"commit %s."), sha1_to_hex(sha1_orig_head));

		if (checkout_fast_forward(sha1_orig_head, sha1_curr_head, 0))
			die(_("Cannot fast-forward your working tree.\n"
			"After making sure that you saved anything precious from\n\n"
			"  git diff %s\n\n"
			"output, run\n\n"
			"  git reset --hard\n\n"
			"to recover."), sha1_to_hex(sha1_orig_head));
	}

	const struct string_list merge_head = get_merge_head();
	if (merge_head.nr == 0)
		error_on_no_merge_candidates(mode, argc, argv);

	if (is_null_sha1(sha1_orig_head))
		return fast_forward_unborn_branch(merge_head);

	if (mode == PULL_MERGE)
		return run_merge(merge_head);

	return run_rebase(merge_head, fork_point_for_rebase, mode);
}
