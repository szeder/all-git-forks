/*
 * 'git stage' builtin command
 *
 * Copyright (C) 2013 Felipe Contreras
 */

#include "builtin.h"
#include "parse-options.h"

static const char *const stage_usage[] = {
	N_("git stage [options] [--] <paths>..."),
	N_("git stage add [options] [--] <paths>..."),
	N_("git stage apply [options] [<patch>...]"),
	N_("git stage reset [-q|--patch] [--] <paths>..."),
	N_("git stage diff [options] [<commit]> [--] <paths>..."),
	N_("git stage rm [options] [--] <paths>..."),
	NULL
};

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
	}

	return cmd_add(argc, argv, prefix);
}
