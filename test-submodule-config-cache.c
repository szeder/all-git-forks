#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "submodule-config-cache.h"

static void die_usage(int argc, char **argv, const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	fprintf(stderr, "Usage: %s [<commit> <submodulepath>] ...\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	char **arg = argv;
	struct submodule_config_cache submodule_config_cache;

	if ((argc - 1) % 2 != 0)
		die_usage(argc, argv, "Wrong number of arguments.");

	submodule_config_cache_init(&submodule_config_cache);

	arg++;
	while (*arg) {
		unsigned char commit_sha1[20];
		struct submodule_config *submodule_config;
		const char *commit;
		const char *path;

		commit = arg[0];
		path = arg[1];

		if (get_sha1(commit, commit_sha1) < 0)
			die_usage(argc, argv, "Commit not found.");

		submodule_config = submodule_config_from_path(&submodule_config_cache,
				commit_sha1, path);
		if (!submodule_config)
			die_usage(argc, argv, "Submodule config not found.");

		printf("Submodule name: '%s' for path '%s'\n", submodule_config->name.buf, path);
		arg += 2;
	}

	submodule_config_cache_free(&submodule_config_cache);

	return 0;
}
