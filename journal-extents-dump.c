#include "git-compat-util.h"
#include "cache.h"
#include "exec_cmd.h"
#include "journal.h"
#include "remote.h"
#include "parse-options.h"
#include "journal-client.h"
#include "journal-util.h"
#include "remote.h"
#include "strbuf.h"

static char const * const journal_extents_dump_usage[] = {
	N_("git journal-extents-dump [--remote <remote>]"),
	NULL
};

static void remote_state_dump(struct remote *upstream)
{
	struct remote_state r;

	printf("Operating against remote '%s'\n",
	       upstream->name);

	remote_state_load(upstream, &r, 0);
	printf("Processed offset is %"PRIu32"\n",
	       r.r.processed_offset);
}

static int journal_extents_dump(const char *journal_dir)
{
	int extent_fd = open_extents_at_dir(journal_dir);
	struct journal_extent_rec e;
	size_t where = 0;

	while (read_in_full(extent_fd, &e, sizeof(e)) == sizeof(e)) {
		printf("%16zu: (%c) %"PRIx32"@%"PRIu32"+%"PRIu32"\n",
		       where,
		       e.opcode, ntohl(e.serial), ntohl(e.offset), ntohl(e.length));
		where = xsize_t(lseek(extent_fd, 0, SEEK_CUR));
	}

	return 0;
}

int main(int argc, const char **argv)
{
	const char *remote_name = NULL;
	const char *journal_dir = NULL;

	struct option opts[] = {
		OPT_GROUP(""),
		OPT_STRING('r', "remote", &remote_name, "remote",
			   "dump the extents file for remote named <remote>"),
		OPT_END()
	};
	git_extract_argv0_path(argv[0]);

	setup_git_directory();

	argc = parse_options(argc, argv, NULL, opts, journal_extents_dump_usage, 0);
	git_config(git_default_config, NULL);

	if (remote_name) {
		struct remote *upstream = remote_get(remote_name);

		remote_state_dump(upstream);
		journal_dir = journal_remote_dir(upstream);
	} else {
		journal_dir = journal_dir_local();
	}

	return journal_extents_dump(journal_dir);
}
