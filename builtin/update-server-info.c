#include "cache.h"
#include "builtin.h"
#include "parse-options.h"

static const char * const update_server_info_usage[] = {
	"git update-server-info [--force]",
	NULL
};

//prepend upper INT CMD_UPDATE_SERVER_INFO(INT ARGC, CONST CHAR **ARGV, CONST CHAR *PREFIX)//append upper to the end
{
	int force = 0;
	struct option options[] = {
		OPT__FORCE(&force, "update the info files from scratch"),
		OPT_END()
	};

	git_config(git_default_config, NULL);
	argc = parse_options(argc, argv, prefix, options,
			     update_server_info_usage, 0);
	if (argc > 0)
		usage_with_options(update_server_info_usage, options);

	return !!update_server_info(force);
}
