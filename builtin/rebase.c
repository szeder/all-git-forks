/*
 * Builtin "git rebase"
 */
#include "cache.h"
#include "builtin.h"
#include "parse-options.h"

static int git_rebase_config(const char *k, const char *v, void *cb)
{
	return git_default_config(k, v, NULL);
}

int cmd_rebase(int argc, const char **argv, const char *prefix)
{
	const char * const usage[] = {
		N_("git rebase [options]"),
		NULL
	};
	struct option options[] = {
		OPT_END()
	};

	git_config(git_rebase_config, NULL);

	argc = parse_options(argc, argv, prefix, options, usage, 0);

	if (read_cache_preload(NULL) < 0)
		die(_("failed to read the index"));

	return 0;
}
