#ifndef REBASE_COMMON_H
#define REBASE_COMMON_H

struct object_id;

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

/**
 * If the work tree has unstaged or uncommitted changes, die() with the
 * appropriate message.
 */
void rebase_die_on_unclean_worktree(void);

/**
 * Do a git reset --hard {oid}
 */
int reset_hard(const struct object_id *);

#endif /* REBASE_COMMON_H */
