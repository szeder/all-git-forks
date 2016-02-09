/*
 * Safe-append files allow atomic appends to files without the
 * possibility for crashes to cause data corruption.  See
 * Documentation/technical/safeappend.txt for details.
 */
#ifndef SAFE_APPEND_H
#define SAFE_APPEND_H

#include <stdint.h>
#include "lockfile.h"

/*
 * Because these contain struct lock_file entries, they must be
 * allocated statically or leaked.
 */
struct safeappend_lock {
	struct lock_file lock_file;
	int fd;
	off_t original_size;
};

int stat_safeappend_file(const char *path, struct stat *st);

int open_safeappend_file(const char *path, int flags, int mode);

int lock_safeappend_file(struct safeappend_lock *ctx, const char *path, int flags);

void rollback_locked_safeappend_file(struct safeappend_lock *ctx);

int commit_safeappend_file(const char *path, int fd);

int commit_locked_safeappend_file(struct safeappend_lock *ctx);

int unlink_safeappend_file(const char *path);
#endif
