#include "git-compat-util.h"
#include "cache.h"
#include "exec_cmd.h"
#include "parse-options.h"
#include "strbuf.h"

static char const * const index_dump_usage[] = {
	N_("git index-dump"),
	NULL
};

void index_dump(void)
{
	int nr = the_index.cache_nr;
	int i;

	for (i = 0; i < nr; ++i) {
		struct cache_entry *ce = the_index.cache[i];
		printf("%d\t%s\t%o\t%o\t%d.%09d\n", ce_stage(ce), ce->name, ce->ce_mode, ce->ce_flags, ce->ce_stat_data.sd_mtime.sec, ce->ce_stat_data.sd_mtime.nsec);
	}
}


int main(int argc, const char **argv)
{
	struct option opts[] = {
		OPT_END()
	};

	git_extract_argv0_path(argv[0]);
	argc = parse_options(argc, argv, NULL, opts, index_dump_usage, 0);
	git_config(git_default_config, NULL);

	if (!read_cache()) {
		die("Could not read index\n");
	}

	index_dump();

	return 0;
}
