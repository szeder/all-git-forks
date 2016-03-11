#include "journal.h"
#include "remote.h"
#include "journal-util.h"
#include "safe-append.h"

/**
 * Common functions used by the journal repair tools
 * (journal-dump, journal-extents-dump).
 */

/**
 * Read and return the serial number from the metadata file.
 */
static int32_t get_serial_from_metadata(const char *journal_dir)
{
	int metadata_fd;
	struct journal_metadata m;

	metadata_fd = open(mkpath("%s/metadata.bin", journal_dir), O_RDONLY);
	if (metadata_fd < 0)
		die_errno("could not open metadata file");

	if (read_in_full(metadata_fd, &m, sizeof(m)) != sizeof(m))
		die_errno("could not read metadata file");

	return ntohl(m.journal_serial);
}

/**
 * Return the local journal directory (for a bare repo).
 */
const char *journal_dir_local(void)
{
	return git_path("objects/journals");
}

/**
 * Open the journal file that's in the specified directory.
 * Returns:
 *   fd on success
 *   dies on error
 */
int open_journal_at_dir(const char *journal_dir, int32_t serial)
{
	int journal_fd;

	journal_fd = open(mkpath("%s/%"PRIx32".bin", journal_dir, serial), O_RDONLY);
	if (journal_fd < 0)
		die_errno("could not open journal file");

	return journal_fd;
}

/**
 * Open the extents file that's in the specified directory.
 * Returns:
 *   fd on success
 *   dies on error
 */
int open_extents_at_dir(const char *journal_dir)
{
	const char *extents_path = mkpath("%s/extents.bin", journal_dir);
	int extents_fd;

	extents_fd = open_safeappend_file(extents_path, O_RDONLY, 0);

	if (extents_fd < 0)
		die_errno("unable to open extents file");

	return extents_fd;
}

/**
 * Return the total number of extent records.
 * Returns:
 *   count on success
 *   dies on error
 */
size_t total_extent_records(int extents_fd)
{
	struct stat sb;

	if (fstat(extents_fd, &sb) != 0)
		die_errno("error fstat'ing extents file");

	return (sb.st_size / sizeof(struct journal_extent_rec));
}

/**
 * Parse the options and return the journal directory, local or remote
 * as appropriate.
 *
 * Returns:
 *   0 on success, sets journal_dir & serial
 *   1 on invalid options
 */
int parse_journal_dir(struct remote *remote, const char *serial_str,
		      const char **journal_dir, int32_t *serial)
{
	if (remote) {
		if (serial_str && sscanf(serial_str, "%"PRIx32, serial) == 1)
			*journal_dir = journal_remote_dir(remote);
		else
			return 1;
	} else {
		if (serial_str)
			return 1;

		*journal_dir = journal_dir_local();
		*serial = get_serial_from_metadata(*journal_dir);
	}

	return 0;
}
