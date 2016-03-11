#include "journal.h"
#include "journal-util.h"
#include "exec_cmd.h"
#include "pack.h"
#include "parse-options.h"
#include "run-command.h"
#include "remote.h"

/*
 * git-journal-dump
 *
 * Reads a journal file and dumps the sha1 of all the objects
 * listed in all the index records.
 *
 * See pack-format.txt for a description of the index file format.
 * See https://schacon.github.io/gitbook/7_the_packfile.html for a more visual
 * explanation of the same.
 */

static const int HEADER_LENGTH = 2*4;
static const int FANOUT_TABLE_LENGTH = 256*4;

static char const * const journal_dump_usage[] = {
	N_("git journal-dump [-r remote -s serial]"),
	NULL
};

static int dump_one_index(int journal_fd, struct journal_extent_rec *e)
{
	struct journal_header h;
	int ret;
	unsigned char *buf;
	unsigned char *objects;
	uint32_t nr_objects;
	int i;

	if (pread_in_full(journal_fd, &h, sizeof(h), e->offset) < sizeof(h))
		die_errno("could not read journal header");
	journal_header_from_wire(&h);

	buf = xmalloc(h.payload_length);
	ret = pread_in_full(journal_fd, buf, h.payload_length, e->offset + sizeof(h));
	if (ret < h.payload_length)
		die_errno("could not read index");

	/* verify that buf indeed contains an index */
	if (*((uint32_t *)buf) != htonl(PACK_IDX_SIGNATURE))
		die("corrupt index record found");

	if (*((uint32_t *)(buf + 4)) != htonl(2))
		die("index versions other than 2 not supported");

	/* last entry in the fanout table contains the number of objects */
	nr_objects = ntohl(*((uint32_t *)(buf + HEADER_LENGTH + FANOUT_TABLE_LENGTH - 4)));
	objects = buf + HEADER_LENGTH + FANOUT_TABLE_LENGTH;

	for (i = 0; i < nr_objects; i++) {
		printf("%s\n", sha1_to_hex(objects + i*20));
	}

	free(buf);
	return 0;
}

static int dump_all_indices(const char *journal_dir, int32_t serial)
{
	int journal_fd;
	int extents_fd;

	struct journal_extent_rec e;

	int error = 0;
	ssize_t ret = -1;

	journal_fd = open_journal_at_dir(journal_dir, serial);
	extents_fd = open_extents_at_dir(journal_dir);

	while (1) {
		ret = read_in_full(extents_fd, &e, sizeof(e));
		if (ret == 0)
			break;

		if (ret < 0)
			die_errno("could not read from extents file");

		journal_extent_record_from_wire(&e);

		/**
		 * When operating against a remote, the extents file may contain
		 * entries for previous journals. Skip them.
		 */
		if (e.serial == serial && e.opcode == JOURNAL_OP_INDEX)
			error = error || dump_one_index(journal_fd, &e);
	}

	return 0;
}

int main(int argc, const char **argv)
{
	int32_t serial = -1;
	int verbose = 0;

	const char *remote = NULL;
	const char *serial_str = NULL;
	const char *journal_dir = NULL;
	struct remote *upstream;

	struct option opts[] = {
		OPT_GROUP(""),
		OPT_STRING('r', "remote", &remote, "remote", "remote to operate against"),
		OPT_STRING('s', "serial", &serial_str, "serial", "serial number of the journal (hex)"),
		OPT__VERBOSE(&verbose, "verbose mode"),
		OPT_END()
	};
	git_extract_argv0_path(argv[0]);

	setup_git_directory();

	parse_options(argc, argv, NULL, opts, journal_dump_usage, 0);

	if (!remote) {
		die ("--remote is required");
	}
	upstream = remote_get(remote);

	if (parse_journal_dir(upstream, serial_str, &journal_dir, &serial) != 0) {
		usage_with_options(journal_dump_usage, opts);
	}

	return dump_all_indices(journal_dir, serial);
}
