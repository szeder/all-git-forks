/**
 * git-journal-identity: Print the current journal serial number and
 * extents offset.
 */

#include "git-compat-util.h"
#include "cache.h"
#include "exec_cmd.h"
#include "parse-options.h"
#include "remote.h"
#include "journal.h"
#include "journal-client.h"

static int verbose = 0;

static char const * const my_usage[] = {
	N_("git journal-identity --remote <remote_name>"),
	NULL
};


void journal_identity(const char *remote_name)
{
	struct remote *upstream;
	struct remote_state r;

	remote_name_sanity_check(remote_name);
	upstream = remote_get(remote_name);
	if (!upstream)
		die("no such remote");

	remote_state_load(upstream, &r, verbose);
	if (r.rec_count == 0)
		die("journal: no extents data");

	extents_current_state(upstream, &r, verbose);
	if (verbose)
		printf("journal_serial=%x last record offset=%u length=%u extents_processed_offset=%"PRIu32"\n",
		       r.last.serial, r.last.offset, r.last.length,
		       r.r.processed_offset);
	else
		printf("journal_serial=%x extents_processed_offset=%"PRIu32"\n",
		       r.last.serial, r.r.processed_offset);
}

int main(int argc, const char **argv)
{
	const char *remote = NULL;

	struct option opts[] = {
		OPT_GROUP(""),
		OPT_STRING(0, "remote", &remote, "remote", "fetch from the remote named <remote_name>"),
		OPT_BOOL(0, "verbose", &verbose, "i love to talk"),
		OPT_END()
	};

	git_extract_argv0_path(argv[0]);

	setup_git_directory();

	argc = parse_options(argc, argv, NULL, opts, my_usage, 0);
	git_config(git_default_config, NULL);

	if (!remote)
		remote = "origin";

	if (!journal_present())
		die("journal: not present");

	journal_identity(remote);

	return 0;
}
