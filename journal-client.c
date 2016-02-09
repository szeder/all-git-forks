#include "cache.h"
#include "remote.h"
#include "journal-client.h"
#include "journal-common.h"
#include "journal.h"
#include "strbuf.h"
#include "lockfile.h"
#include "safe-append.h"

const char *extents_path(struct remote *upstream)
{
	return mkpath("%s/extents.bin", journal_remote_dir(upstream));
}

const char *remote_state_path(struct remote *upstream)
{
	return mkpath("%s/state.bin", journal_remote_dir(upstream));
}

int extents_current_state(struct remote *upstream,
			  struct remote_state *r,
			  int verbose)
{
	off_t where;
	int extent_fd;
	const char *ext_path;
	struct stat sb;

	r->rec_count = 0;
	memset(&r->last, 0, sizeof(r->last));

	ext_path = extents_path(upstream);

	if (stat_safeappend_file(ext_path, &sb) != 0) {
		if (errno == ENOENT)
			return 0;
		else
			return -1;
	}

	r->rec_count = sb.st_size / sizeof(r->last);
	if (verbose)
		printf("Extents: %jdB with %zu records.\n",
		       (uintmax_t)sb.st_size, r->rec_count);

	if (r->rec_count == 0)
		return 0;

	/* Read the last extents record from the file if there is one. */
	extent_fd = open_safeappend_file(ext_path, O_RDONLY, 0);
	if (extent_fd < 0)
		die_errno("open failed: extents");

	if (sb.st_size < sizeof(r->last) ||
	    r->r.processed_offset < sizeof(r->last))
		where = 0;
	else
		where = r->r.processed_offset - sizeof(r->last);

	if (where > sb.st_size)
		die_errno("We've processed up to %u in extents, but we only have up to %"PRIuMAX, r->r.processed_offset, (uintmax_t) sb.st_size);

	if (lseek(extent_fd, where, SEEK_SET) != where)
		die_errno("seek failed: extents");

	if (where)
		read_extent_rec(extent_fd, &r->last);

	close(extent_fd);

	return 0;
}

void remote_state_load(struct remote *upstream, struct remote_state * const state, int verbose)
{
	struct remote_state r;
	int fd;
	const char *path = remote_state_path(upstream);

	memset(&r, 0, sizeof(r));

	r.lock = xcalloc(1, sizeof(*r.lock));
	hold_lock_file_for_update(r.lock, path, LOCK_DIE_ON_ERROR);

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		if (errno == ENOENT) {
			/* If the state doesn't exist, it's okay. */
			goto done;
		} else {
			die_errno("open failed: remote state for %s", upstream->name);
		}
	}

	if (read_in_full(fd, &r.r, sizeof(r.r)) != sizeof(r.r))
		die_errno("Error loading remote state for %s", upstream->name);

	r.r.processed_offset = ntohl(r.r.processed_offset);

	close(fd);

 done:
	memcpy(state, &r, sizeof(r));
}

void remote_state_store(struct remote *upstream, struct remote_state *state, int close)
{
	struct remote_state_rec r = state->r;
	int fd;
	char *path;

	assert(state->lock);
	if (lseek(state->lock->tempfile.fd, 0, SEEK_SET))
		die_errno("Failed to seek in state file");

	fd = state->lock->tempfile.fd;

	if (fd < 0)
		die_errno("open for write failed: remote state");

	r.processed_offset = htonl(r.processed_offset);
	if (write_in_full(fd, &r, sizeof(r)) != sizeof(r))
		die_errno("Error storing remote state for repo %s", upstream->name);

	path = get_locked_file_path(state->lock);

	if (commit_lock_file(state->lock))
		die("Failed to store remote state");

	/*
	 * There's a little race here where another process could jump
	 * in and steal the lock out from under us.  If this happens,
	 * our fetch just aborts, and theirs goes through.  Everything
	 * works out.
	 */

	if (!close)
		hold_lock_file_for_update(state->lock, path, LOCK_DIE_ON_ERROR);

	free(path);
}

void remote_name_sanity_check(const char *remote_name)
{
	size_t l = strlen(remote_name);
	size_t i;

	for (i = 0; i < l; ++i) {
		const char c = remote_name[i];
		if ((c >= 'A' && c <= 'Z') ||
		    (c >= 'a' && c <= 'z') ||
		    (c >= '0' && c <= '9') ||
		    (c == '_' || c == '-' || c == '.'))
			continue;
		die("remote name contains invalid character '%c' (allowable: [0-9A-Z-a-z\\-_\\.])",
		    c);
	}
}

int journal_present(void)
{
	static struct stat sb;

	return (stat(journal_dir(), &sb) == 0) && sb.st_mode & S_IFDIR;
}

void read_extent_rec(int extent_fd, struct journal_extent_rec *rec)
{
	errno = 0;
	if (read_in_full(extent_fd, rec, sizeof(*rec)) != sizeof(*rec)) {
		if (errno)
			die_errno("read failed: extents");
		else
			die("read failed: extents: not enough data");
	}

	journal_extent_record_from_wire(rec);
}
