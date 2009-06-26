#include "builtin.h"
#include "cache.h"
#include "parse-options.h"

static const char * const git_sequencer_helper_usage[] = {
	"git sequencer--helper --make-patch <commit>",
	NULL
};

int cmd_sequencer__helper(int argc, const char **argv, const char *prefix)
{
	char *commit = NULL;
	struct option options[] = {
		OPT_STRING(0, "make-patch", &commit, "commit",
			   "create a patch from commit"),
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options,
			     git_sequencer_helper_usage, 0);

	if (!commit)
		usage_with_options(git_sequencer_helper_usage, options);

	/* Nothing to do yet */
	return 0;
}
