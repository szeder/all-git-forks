#ifndef REBASE_COMMON_H
#define REBASE_COMMON_H

/**
 * Refresh and write index.
 */
void refresh_and_write_cache(unsigned int);

/**
 * Returns 1 if there are unstaged changes, 0 otherwise.
 */
int cache_has_unstaged_changes(void);

/**
 * Returns 1 if there are uncommitted changes, 0 otherwise.
 */
int cache_has_uncommitted_changes(void);

#endif /* REBASE_COMMON_H */
