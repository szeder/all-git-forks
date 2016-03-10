/*
 * Builtin "git rebase"
 */
#include "cache.h"
#include "builtin.h"
#include "parse-options.h"
#include "rebase-common.h"
#include "remote.h"
#include "branch.h"
#include "refs.h"

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

static int git_rebase_config(const char *k, const char *v, void *cb)
{
	return git_default_config(k, v, NULL);
}

int cmd_rebase(int argc, const char **argv, const char *prefix)
{
	struct rebase_options rebase_opts;
	const char *onto_name = NULL;
	const char *branch_name;

	const char * const usage[] = {
		N_("git rebase [options] [--onto <newbase>] [<upstream>] [<branch>]"),
		NULL
	};
	struct option options[] = {
		OPT_GROUP(N_("Available options are")),
		OPT_STRING(0, "onto", &onto_name, NULL,
			N_("rebase onto given branch instead of upstream")),
		OPT_END()
	};

	git_config(git_rebase_config, NULL);
	rebase_options_init(&rebase_opts);
	rebase_opts.resolvemsg = _("\nWhen you have resolved this problem, run \"git rebase --continue\".\n"
			"If you prefer to skip this patch, run \"git rebase --skip\" instead.\n"
			"To check out the original branch and stop rebasing, run \"git rebase --abort\".");

	argc = parse_options(argc, argv, prefix, options, usage, 0);

	/*
	 * Parse command-line arguments:
	 *    rebase [<options>] [<upstream_name>] [<branch_name>]
	 */

	/* Parse <upstream_name> into rebase_opts.upstream */
	{
		const char *upstream_name;
		if (argc > 2)
			usage_with_options(usage, options);
		if (!argc) {
			upstream_name = get_curr_branch_upstream_name();
		} else {
			upstream_name = argv[0];
			argv++, argc--;
			if (!strcmp(upstream_name, "-"))
				upstream_name = "@{-1}";
		}
		if (get_oid_commit(upstream_name, &rebase_opts.upstream))
			die(_("invalid upstream %s"), upstream_name);
		if (!onto_name)
			onto_name = upstream_name;
	}

	/*
	 * Parse --onto <onto_name> into rebase_opts.onto and
	 * rebase_opts.onto_name
	 */
	get_onto_oid(onto_name, &rebase_opts.onto);
	rebase_opts.onto_name = xstrdup(onto_name);

	/*
	 * Parse <branch_name> into rebase_opts.orig_head and
	 * rebase_opts.orig_refname
	 */
	branch_name = argv[0];
	if (branch_name) {
		/* Is branch_name a branch or commit? */
		char *ref_name = xstrfmt("refs/heads/%s", branch_name);
		struct object_id orig_head_id;

		if (!read_ref(ref_name, orig_head_id.hash)) {
			rebase_opts.orig_refname = ref_name;
			if (get_oid_commit(ref_name, &rebase_opts.orig_head))
				die("get_sha1_commit failed");
		} else if (!get_oid_commit(branch_name, &rebase_opts.orig_head)) {
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

		if (get_oid_commit("HEAD", &rebase_opts.orig_head))
			die(_("Failed to resolve '%s' as a valid revision."), "HEAD");
	}

	rebase_options_release(&rebase_opts);
	return 0;
}
