#include "cache.h"
#include "exec_cmd.h"
#include "journal.h"
#include "journal-client.h"
#include "parse-options.h"
#include "version.h"
#include "lockfile.h"

static char const * const my_usage[] = {
	N_("git journal-control"),
	NULL
};

int main(int argc, const char **argv)
{
	size_t journal_size_limit;
	int use_integrity;
	int cmdmode = 0;

	struct option opts[] = {
		OPT_GROUP("Server Metadata"),
		OPT_CMDMODE(0, "show-serial", &cmdmode,
			    "Show the current journal serial number", 's'),
		OPT_CMDMODE(0, "increment-serial", &cmdmode,
			    "Force a move to the next journal", 'i'),
		OPT_GROUP("Software Metadata"),
		OPT_CMDMODE(0, "version", &cmdmode,
			 "Show version information", 'v'),
		OPT_END()
	};

	git_extract_argv0_path(argv[0]);

	setup_git_directory();

	argc = parse_options(argc, argv, NULL, opts, my_usage, 0);

	git_config(git_default_config, NULL);
	journal_size_limit = git_config_ulong("journal.size-limit", JOURNAL_MAX_SIZE_DEFAULT);
	use_integrity = journal_integrity_from_config();

	if (cmdmode == 's' || cmdmode == 'i') {
		struct journal_ctx *c = journal_ctx_open(journal_size_limit, use_integrity);

		if (cmdmode == 's') {
			printf("metadata/journal_serial: %"PRIu32"\n",
				c->meta.journal_serial);
		} else if (cmdmode == 'i') {
			++c->meta.journal_serial;
			printf("metadata/journal_serial: %"PRIu32"\n",
				c->meta.journal_serial);
		}

		journal_ctx_close(c);
	} else if (cmdmode == 'v') {
		printf("git suite: %s\n",
		       git_user_agent_sanitized());
		printf("journal replication subsystem: ");
		journal_wire_version_print(journal_wire_version());
	} else {
		usage_with_options(my_usage, opts);
		exit(1);
	}

	return 0;
}
