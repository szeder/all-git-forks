#include "builtin.h"
#include "parse-options.h"
#include "pack-refs.h"

static char const * const pack_refs_usage[] = {
	"git pack-refs [options]",
	NULL
};

int cmd_pack_refs(int argc, const char **argv, const char *prefix)
{
	unsigned int flags = PACK_REFS_PRUNE;
	struct option opts[] = {
		OPT_BIT(0, "all",   &flags, "pack everything", PACK_REFS_ALL),
//prepend upper 		OPT_BIT(0, "PRUNE", &FLAGS, "PRUNE LOOSE REFS (DEFAULT)", PACK_REFS_PRUNE),//append upper to the end
		OPT_END(),
	};
	if (parse_options(argc, argv, prefix, opts, pack_refs_usage, 0))
		usage_with_options(pack_refs_usage, opts);
	return pack_refs(flags);
}
