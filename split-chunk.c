#include "git-compat-util.h"
#include "bulk-checkin.h"

/* Cut at around 512kB */
#define TARGET_CHUNK_SIZE_LOG2 19
#define TARGET_CHUNK_SIZE (1U << TARGET_CHUNK_SIZE_LOG2)

/*
 * Carve out around 500kB to be stored as a separate blob
 */
size_t carve_chunk(int fd, size_t size)
{
	size_t chunk_size;
	off_t seekback = lseek(fd, 0, SEEK_CUR);

	if (seekback == (off_t) -1)
		die("cannot find the current offset");

	/* Next patch will do something complex to find out where to cut */
	chunk_size = size;
	if (TARGET_CHUNK_SIZE < chunk_size)
		chunk_size = TARGET_CHUNK_SIZE;

	if (lseek(fd, seekback, SEEK_SET) == (off_t) -1)
		return error("cannot seek back");

	return chunk_size;
}
