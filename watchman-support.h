#ifndef WATCHMAN_SUPPORT_H
#define WATCHMAN_SUPPORT_H

#include "cache.h"
#include <watchman.h>

/**
 * The async load function takes a continuation which it executes on the
 * calling thread, while simultaneously (re)loading the FS cache on a background
 * thread. These functions assume that the continuation will complete in about
 * as much time as it takes to load the FS cache from the disk, and optimizes
 * for this case - for e.g. by spinning on a variable instead of parking the
 * calling thread, to synchronize.
 *
 * In the reload case, it is illegal to modify the fs_cache instance within
 * the continuation while the FS cache is being reloaded.
 *
 * @continuation A function that is executed in parallel with the FS cache
 *               loader.
 * @context      Caller's context that is passed unmodified to the continuation.
 *
 */
void watchman_async_load_fs_cache(void (*continuation)(void *context), void *context);

/**
 * synchronously load the_fs_cache
 */
void load_fs_cache(void);

/**
 * tell watchman to abandon its state of the world and reexamine the filesystem.
 * Returns 0 on success and -1 on failure
 */
int watchman_do_recrawl(void);

/**
 * Get the clock marker for now and set the_fs_cache.last_updated to
 * that time. This has the effect of skipping over any pending
 * watchman updates that we have not queried. Returns 0 on success
 * and -1 on failure
 */
int watchman_fast_forward_clock(void);

/**
 * Check if Watchman is running, and if not, spawn it.
 */
void check_run_watchman(void);

#endif /* WATCHMAN_SUPPORT_H */
