/*
 * Copyright (c) 2005, Junio C Hamano
 */
#include "cache.h"
#include "sigchain.h"
#include "tempfile.h"

/*
 * File write-locks as used by Git.
 *
 * When a file at $FILENAME needs to be written, it is done as
 * follows:
 *
 * 1. Obtain a lock on the file by creating a lockfile at path
 *    $FILENAME.lock.  The file is opened for read/write using O_CREAT
 *    and O_EXCL mode to ensure that it doesn't already exist.  Such a
 *    lock file is respected by writers *but not by readers*.
 *
 *    Usually, if $FILENAME is a symlink, then it is resolved, and the
 *    file ultimately pointed to is the one that is locked and later
 *    replaced.  However, if LOCK_NODEREF is used, then $FILENAME
 *    itself is locked and later replaced, even if it is a symlink.
 *
 * 2. Write the new file contents to the lockfile.
 *
 * 3. Move the lockfile to its final destination using rename(2).
 *
 * Instead of (3), the change can be rolled back by deleting lockfile.
 *
 * This module keeps track of all locked files in lock_file_list.
 * When the first file is locked, it registers an atexit(3) handler
 * and a signal handler; when the program exits, the handler rolls
 * back any files that have been locked but were never committed or
 * rolled back.
 *
 * Because the signal handler can be called at any time, a lock_file
 * object must always be in a well-defined state.  The possible states
 * are as follows:
 *
 * - Uninitialized.  In this state the object's on_list field must be
 *   zero but the rest of its contents need not be initialized.  As
 *   soon as the object is used in any way, it is irrevocably
 *   registered in the lock_file_list, and on_list is set.
 *
 * - Locked, lockfile open (after hold_lock_file_for_update() or
 *   hold_lock_file_for_append()).  In this state:
 *   - the lockfile exists
 *   - active is set
 *   - filename holds the filename of the lockfile
 *   - fd holds a file descriptor open for writing to the lockfile
 *   - owner holds the PID of the process that locked the file
 *
 * - Locked, lockfile closed (after close_lock_file() or an
 *   unsuccessful commit_lock_file()).  Same as the previous state,
 *   except that the lockfile is closed and fd is -1.
 *
 * - Unlocked (after commit_lock_file(), rollback_lock_file(), or a
 *   failed attempt to lock).  In this state, filename[0] == '\0' and
 *   fd is -1.  The object is left registered in the lock_file_list,
 *   and on_list is set.
 * - Unlocked (after rollback_lock_file(), a successful
 *   commit_lock_file(), or a failed attempt to lock).  In this state:
 *   - active is unset
 *   - filename[0] == '\0' (usually, though there are transitory states
 *     in which this condition doesn't hold)
 *   - fd is -1
 *   - the object is left registered in the lock_file_list, and
 *     on_list is set.
 *
 * See Documentation/api-lockfile.txt for more information.
 */

static char *unable_to_lock_message(const char *path, int err)
{
	struct strbuf buf = STRBUF_INIT;

	if (err == EEXIST) {
		strbuf_addf(&buf, "Unable to create '%s.lock': %s.\n\n"
		    "If no other git process is currently running, this probably means a\n"
		    "git process crashed in this repository earlier. Make sure no other git\n"
		    "process is running and remove the file manually to continue.",
			    absolute_path(path), strerror(err));
	} else
		strbuf_addf(&buf, "Unable to create '%s.lock': %s",
			    absolute_path(path), strerror(err));
	return strbuf_detach(&buf, NULL);
}

int unable_to_lock_error(const char *path, int err)
{
	char *msg = unable_to_lock_message(path, err);
	error("%s", msg);
	free(msg);
	return -1;
}

NORETURN void unable_to_lock_die(const char *path, int err)
{
	die("%s", unable_to_lock_message(path, err));
}

int initialize_lock_file(struct temp_file *tmp, const char *path, int flags)
{
	struct strbuf locked = STRBUF_INIT;
	strbuf_addstr(&locked, path);
	strbuf_addstr(&locked, LOCK_SUFFIX);

	int fd = initialize_temp_file(tmp, locked.buf, path, flags);
	strbuf_reset(&locked);

	return fd;
}

int hold_lock_file_for_update(struct temp_file *tmp, const char *path, int flags)
{
	int fd = initialize_lock_file(tmp, path, flags);
	if (fd < 0 && (flags & LOCK_DIE_ON_ERROR))
		unable_to_lock_die(path, errno);
	return fd;
}

int hold_lock_file_for_append(struct temp_file *tmp, const char *path, int flags)
{
	int fd, orig_fd;

	fd = initialize_lock_file(tmp, path, flags);
	if (fd < 0) {
		if (flags & LOCK_DIE_ON_ERROR)
			unable_to_lock_die(path, errno);
		return fd;
	}

	orig_fd = open(path, O_RDONLY);
	if (orig_fd < 0) {
		if (errno != ENOENT) {
			if (flags & LOCK_DIE_ON_ERROR)
				die("cannot open '%s' for copying", path);
			rollback_temp_file(tmp);
			return error("cannot open '%s' for copying", path);
		}
	} else if (copy_fd(orig_fd, fd)) {
		if (flags & LOCK_DIE_ON_ERROR)
			exit(128);
		rollback_temp_file(tmp);
		return -1;
	}
	return fd;
}

int hold_locked_index(struct temp_file *tmp, int die_on_error)
{
	return hold_lock_file_for_update(tmp, get_index_file(),
					 die_on_error
					 ? LOCK_DIE_ON_ERROR
					 : 0);
}

int close_lock_file(struct lock_file *lk)
{
	return close_temp_file((struct temp_file *)lk);
}
