/*
 * "git submodule" builtin command
 *
 * Copyright (C) 2012 Fredrik Gustafsson
 */
#include "builtin.h"

static const char * const submodule_usage[] = {
	N_("git submodule [<options>...] [--] [<pathspec>...]"),
	NULL
};

static int cmd_submodule_status()
{
	printf("Doing submodule status\n");
	//struct rev_info rev;
	//init_revisions(&rev, NULL);
	return 0;
}

int cmd_submodule(int argc, const char **argv, const char *prefix)
{
	int exit_status = 0;
	/*
	int opt_short = 0, opt_branch = 0, opt_porcelain = 0,
	    opt_untracked_files = 0, opt_ignore_submodule = 0,
	    opt_z = 0;

	struct option options[] = {
		OPT_END()
	};
	*/

	printf("Submodule!!!\n");
	//argc = parse_options(argc, argv, prefix, options, submodule_usage, 0);

	exit_status = cmd_submodule_status();

	return exit_status;
}
