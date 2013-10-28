/*
 * 'git stage' builtin command
 *
 * Copyright (C) 2013 Felipe Contreras
 */

#include "builtin.h"
#include "parse-options.h"
#include "diff.h"
#include "diffcore.h"
#include "revision.h"

static const char *const stage_usage[] = {
	N_("git stage [options] [--] <paths>..."),
	N_("git stage add [options] [--] <paths>..."),
	N_("git stage apply [options] [<patch>...]"),
	N_("git stage reset [-q|--patch] [--] <paths>..."),
	N_("git stage diff [options] [<commit]> [--] <paths>..."),
	N_("git stage rm [options] [--] <paths>..."),
	NULL
};

static int do_reset(const char *prefix)
{
	const char *argv[] = { "reset", "--quiet", NULL };
	return cmd_reset(2, argv, prefix);
}

static int do_apply(const char *file, const char *prefix)
{
	const char *argv[] = { "apply", "--recount", "--cached", file, NULL };
	return cmd_apply(4, argv, prefix);
}

static int edit(int argc, const char **argv, const char *prefix)
{
	char *file = git_pathdup("STAGE_EDIT.patch");
	int out;
	struct rev_info rev;
	int ret = 0;
	struct stat st;

	read_cache();

	init_revisions(&rev, prefix);
	rev.diffopt.context = 7;

	argc = setup_revisions(argc, argv, &rev, NULL);
	add_head_to_pending(&rev);
	if (!rev.pending.nr) {
		struct tree *tree;
		tree = lookup_tree(EMPTY_TREE_SHA1_BIN);
		add_pending_object(&rev, &tree->object, "HEAD");
	}

	rev.diffopt.output_format = DIFF_FORMAT_PATCH;
	rev.diffopt.use_color = 0;
	DIFF_OPT_SET(&rev.diffopt, IGNORE_DIRTY_SUBMODULES);

	out = open(file, O_CREAT | O_WRONLY, 0666);
	if (out < 0)
		die(_("Could not open '%s' for writing."), file);
	rev.diffopt.file = xfdopen(out, "w");
	rev.diffopt.close_file = 1;

	if (run_diff_index(&rev, 1))
		die(_("Could not write patch"));
	if (launch_editor(file, NULL, NULL))
		exit(1);

	if (stat(file, &st))
		die_errno(_("Could not stat '%s'"), file);

	ret = do_reset(prefix);
	if (ret)
		goto leave;

	if (!st.st_size)
		goto leave;

	ret = do_apply(file, prefix);
	if (ret)
		goto leave;

leave:
	unlink(file);
	free(file);
	return ret;
}

int cmd_stage(int argc, const char **argv, const char *prefix)
{
	struct option options[] = { OPT_END() };

	argc = parse_options(argc, argv, prefix, options, stage_usage,
			PARSE_OPT_KEEP_ARGV0 | PARSE_OPT_KEEP_UNKNOWN | PARSE_OPT_KEEP_DASHDASH);

	if (argc > 1) {
		if (!strcmp(argv[1], "add"))
			return cmd_add(argc - 1, argv + 1, prefix);
		if (!strcmp(argv[1], "reset"))
			return cmd_reset(argc - 1, argv + 1, prefix);
		if (!strcmp(argv[1], "diff")) {
			argv[0] = "diff";
			argv[1] = "--staged";

			return cmd_diff(argc, argv, prefix);
		}
		if (!strcmp(argv[1], "rm")) {
			argv[0] = "rm";
			argv[1] = "--cached";

			return cmd_rm(argc, argv, prefix);
		}
		if (!strcmp(argv[1], "apply")) {
			argv[0] = "apply";
			argv[1] = "--cached";

			return cmd_apply(argc, argv, prefix);
		}
		if (!strcmp(argv[1], "edit")) {
			return edit(argc - 1, argv + 1, prefix);
		}
	}

	return cmd_add(argc, argv, prefix);
}
