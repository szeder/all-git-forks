#include "builtin.h"
#include "cache.h"
#include "parse-options.h"
#include "quote.h"
#include "pathspec.h"
#include "dir.h"
#include "utf8.h"
#include "submodule.h"
#include "submodule-config.h"
#include "string-list.h"

static const struct cache_entry **ce_entries;
static int ce_alloc, ce_used;
static const char *alternative_path;

static int module_list_compute(int argc, const char **argv,
				const char *prefix,
				struct pathspec *pathspec)
{
	int i;
	char *max_prefix, *ps_matched = NULL;
	int max_prefix_len;
	parse_pathspec(pathspec, 0,
		       PATHSPEC_PREFER_FULL |
		       PATHSPEC_STRIP_SUBMODULE_SLASH_CHEAP,
		       prefix, argv);

	/* Find common prefix for all pathspec's */
	max_prefix = common_prefix(pathspec);
	max_prefix_len = max_prefix ? strlen(max_prefix) : 0;

	if (pathspec->nr)
		ps_matched = xcalloc(pathspec->nr, 1);

	if (read_cache() < 0)
		die("index file corrupt");

	for (i = 0; i < active_nr; i++) {
		const struct cache_entry *ce = active_cache[i];

		if (!match_pathspec(pathspec, ce->name, ce_namelen(ce),
				    max_prefix_len, ps_matched,
				    S_ISGITLINK(ce->ce_mode) | S_ISDIR(ce->ce_mode)))
			continue;

		if (S_ISGITLINK(ce->ce_mode)) {
			ALLOC_GROW(ce_entries, ce_used + 1, ce_alloc);
			ce_entries[ce_used++] = ce;
		}

		while (i + 1 < active_nr && !strcmp(ce->name, active_cache[i + 1]->name))
			/*
			 * Skip entries with the same name in different stages
			 * to make sure an entry is returned only once.
			 */
			i++;
	}
	free(max_prefix);

	if (ps_matched && report_path_error(ps_matched, pathspec, prefix))
		return -1;

	return 0;
}

static int module_list(int argc, const char **argv, const char *prefix)
{
	int i;
	static struct pathspec pathspec;

	struct option module_list_options[] = {
		OPT_STRING(0, "prefix", &alternative_path,
			   N_("path"),
			   N_("alternative anchor for relative paths")),
		OPT_END()
	};

	static const char * const git_submodule_helper_usage[] = {
		N_("git submodule--helper module_list [--prefix=<path>] [<path>...]"),
		NULL
	};

	argc = parse_options(argc, argv, prefix, module_list_options,
			     git_submodule_helper_usage, 0);

	if (module_list_compute(argc, argv, alternative_path
					    ? alternative_path
					    : prefix, &pathspec) < 0) {
		printf("#unmatched\n");
		return 1;
	}

	for (i = 0; i < ce_used; i++) {
		const struct cache_entry *ce = ce_entries[i];

		if (ce_stage(ce)) {
			printf("%06o %s U\t", ce->ce_mode, sha1_to_hex(null_sha1));
		} else {
			printf("%06o %s %d\t", ce->ce_mode, sha1_to_hex(ce->sha1), ce_stage(ce));
		}

		utf8_fprintf(stdout, "%s\n", ce->name);
	}
	return 0;
}

static int module_name(int argc, const char **argv, const char *prefix)
{
	const char *name;
	const struct submodule *sub;

	if (argc != 1)
		usage("git submodule--helper module_name <path>\n");

	gitmodules_config();
	sub = submodule_from_path(null_sha1, argv[0]);

	if (!sub)
		die("No submodule mapping found in .gitmodules for path '%s'", argv[0]);

	name = sub->name;
	printf("%s\n", name);

	return 0;
}

int cmd_submodule__helper(int argc, const char **argv, const char *prefix)
{
	if (argc < 2)
		goto usage;

	if (!strcmp(argv[1], "module_list"))
		return module_list(argc - 1, argv + 1, prefix);

	if (!strcmp(argv[1], "module_name"))
		return module_name(argc - 2, argv + 2, prefix);

usage:
	usage("git submodule--helper [module_list module_name]\n");
}
