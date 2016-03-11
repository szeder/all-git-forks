#include "cache.h"
#include "exec_cmd.h"
#include "journal.h"
#include "journal-client.h"
#include "parse-options.h"
#include "version.h"
#include "journal-connectivity.h"
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
		OPT_CMDMODE(0, "packlog-append", &cmdmode,
			    "Add hashes from STDIN to the packlog (hexadecimal, endline-separated)", 'p'),
		OPT_CMDMODE(0, "packlog-dump", &cmdmode,
				"dump the hashes from the packlog to STDOUT", 'd'),
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
	} else if (cmdmode == 'p') {
#ifdef USE_LIBLMDB
		char *line = NULL;
		size_t linecap = 0;
		ssize_t linelen;
		unsigned char sha1[20];
		size_t line_count = 0;
		size_t add_count = 0;
		struct jcdb_transaction transaction;
		jcdb_transaction_begin(&transaction, JCDB_CREATE);

		while ((linelen = getline(&line, &linecap, stdin)) > 0) {
			++line_count;
			if (!get_sha1_hex(line, sha1)) {
				++add_count;
				jcdb_add_pack(&transaction, sha1);
			} else {
				warning("invalid hash at line %zu data: %s", line_count, line);
			}
		}
		jcdb_transaction_commit(&transaction);
#else
		die(_("git was not built with liblmdb"));
#endif
	} else if (cmdmode == 'd') {
#ifdef USE_LIBLMDB
		jcdb_packlog_dump();
#else
		die(_("git was not built with liblmdb"));
#endif

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
