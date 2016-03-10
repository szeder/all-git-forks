#include "cache.h"
#include "rebase-common.h"
#include "lockfile.h"

void refresh_and_write_cache(void)
{
	struct lock_file *lock_file = xcalloc(1, sizeof(struct lock_file));

	hold_locked_index(lock_file, 1);
	refresh_cache(REFRESH_QUIET);
	if (write_locked_index(&the_index, lock_file, COMMIT_LOCK))
		die(_("unable to write index file"));
}
