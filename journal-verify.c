#include "journal.h"
#include "exec_cmd.h"
#include "parse-options.h"
#include "progress.h"
#include "remote.h"
#include "journal-util.h"

#define ERROR_STR(e)	((e) ? "err" : "ok")

static char const * const journal_verify_usage[] = {
	N_("git journal-verify [-e] [-v] [-r remote -s serial]"),
	NULL
};

/* command line options */
static int verbose;

static void validate_opcode(const struct journal_extent_rec *e)
{
	if (!((e->opcode == JOURNAL_OP_INDEX) ||
	     (e->opcode == JOURNAL_OP_PACK) ||
	     (e->opcode == JOURNAL_OP_REF) ||
	     (e->opcode == JOURNAL_OP_UPGRADE))) {
		printf("err extent record @%"PRIu32" invalid opcode %c\n", ntohl(e->offset), e->opcode);
	}
}

static int verify_extent_record(int journal_fd, const struct journal_extent_rec *e)
{
	struct journal_header h;
	uint32_t payload_length;

	uint32_t journal_record_length = ntohl(e->length);
	uint32_t journal_offset = ntohl(e->offset);

	int error = 0;
	char *error_msg = "";

	validate_opcode(e);

	if (pread_in_full(journal_fd, &h, sizeof(h), journal_offset) < 0)
		die_errno("could not read journal header");

	if (e->opcode != h.opcode) {
		error = 1;
		error_msg = "mismatched opcode";
		goto done;
	}

	payload_length = ntohl(h.payload_length);

	if (journal_record_length != payload_length + sizeof(h)) {
		error = 1;
		error_msg = "incorrect payload length";
		goto done;
	}

done:
	if (verbose || error) {
		printf("%s ", ERROR_STR(error));

		printf("extent: (%c) %"PRIx32"@%"PRIu32"+%"PRIu32" ",
		       e->opcode, ntohl(e->serial), journal_offset, journal_record_length);

		printf("journal: (%c) @%"PRIu32"+(%"PRIu32"+%"PRIu32") sha1=%s ",
		       h.opcode, journal_offset,
		       (uint32_t) sizeof(h), payload_length,
		       sha1_to_hex((const unsigned char *) &h.sha));

		printf("%s\n", error_msg);
	}

	return error;
}

static int journal_verify_extents(const char *journal_dir, int32_t serial)
{
	int journal_fd;
	int extents_fd;

	struct journal_extent_rec e;

	uint32_t prev_offset;
	uint32_t prev_length;

	uint32_t records_processed = 0;

	int error = 0;
	ssize_t ret = -1;

	struct progress *progress;

	journal_fd = open_journal_at_dir(journal_dir, serial);
	extents_fd = open_extents_at_dir(journal_dir);

	progress = start_progress("Verifying extent records", total_extent_records(extents_fd));

	while (1) {
		ret = read_in_full(extents_fd, &e, sizeof(e));
		if (ret == 0)
			break;

		if (ret < 0)
			die_errno("could not read from extents file");

		if (records_processed > 0) {
			if (ntohl(e.offset) != prev_offset + prev_length) {
				printf("err extent record number %"PRIu32" offset = %"PRIu32
				       " prev_offset = %"PRIu32" prev_length = %"PRIu32" not strictly contiguous\n",
				records_processed, ntohl(e.offset), prev_offset, prev_length);
			}
		}

		prev_offset = ntohl(e.offset);
		prev_length = ntohl(e.length);

		records_processed++;
		display_progress(progress, records_processed);

		/**
		 * When operating against a remote, the extents file may contain
		 * entries for previous journals. Skip them.
		 */
		if (ntohl(e.serial) == serial)
			error = error || verify_extent_record(journal_fd, &e);
	}

	stop_progress(&progress);

	return error;
}

int main(int argc, const char **argv)
{
	int extents = 1;
	int32_t serial = -1;

	const char *remote = NULL;
	const char *serial_str = NULL;
	const char *journal_dir = NULL;
	struct remote *upstream;

	struct option opts[] = {
		OPT_GROUP(""),
		OPT_STRING('r', "remote", &remote, "remote", "remote to operate against"),
		OPT_STRING('s', "serial", &serial_str, "serial", "serial number of the journal (hex)"),
		OPT_BOOL('e', "extents", &extents, "verify that the journal matches the extents file"),
		OPT__VERBOSE(&verbose, "verbose mode"),
		OPT_END()
	};

	git_extract_argv0_path(argv[0]);

	setup_git_directory();

	parse_options(argc, argv, NULL, opts, journal_verify_usage, 0);

	upstream = remote_get(remote);

	if (parse_journal_dir(upstream, serial_str, &journal_dir, &serial) != 0) {
		usage_with_options(journal_verify_usage, opts);
	}

	return journal_verify_extents(journal_dir, serial);
}
