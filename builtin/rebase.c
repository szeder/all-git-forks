/*
 * Builtin "git rebase"
 */
#include "cache.h"
#include "builtin.h"
#include "parse-options.h"

int cmd_rebase(int argc, const char **argv, const char *prefix)
{
	const char * const usage[] = {
		N_("git rebase [<options>]"),
		NULL
	};

	struct option options[] = {
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options, usage, 0);

	return 0;
}
