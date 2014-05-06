/*
 * Copyright (c) 2005, Junio C Hamano
 */
#include "cache.h"
#include "sigchain.h"

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

static struct lock_file *volatile lock_file_list;

static void remove_lock_file(void)
{
	pid_t me = getpid();

	while (lock_file_list) {
		if (lock_file_list->owner == me)
			rollback_lock_file(lock_file_list);
		lock_file_list = lock_file_list->next;
	}
}

static void remove_lock_file_on_signal(int signo)
{
	remove_lock_file();
	sigchain_pop(signo);
	raise(signo);
}

/*
 * path = absolute or relative path name
 *
 * Remove the last path name element from path (leaving the preceding
 * "/", if any).  If path is empty or the root directory ("/"), set
 * path to the empty string.
 */
static void trim_last_path_elm(struct strbuf *path)
{
	int i = path->len;

	/* back up past trailing slashes, if any */
	while (i && path->buf[i - 1] == '/')
		i--;

	/*
	 * then go backwards until a slash, or the beginning of the
	 * string
	 */
	while (i && path->buf[i - 1] != '/')
		i--;

	strbuf_setlen(path, i);
}


/* We allow "recursive" symbolic links. Only within reason, though */
#define MAXDEPTH 5

/*
 * path contains a path that might be a symlink.
 *
 * If path is a symlink, attempt to overwrite it with a path to the
 * real file or directory (which may or may not exist), following a
 * chain of symlinks if necessary.  Otherwise, leave path unmodified.
 *
 * This is a best-effort routine.  If an error occurs, path will
 * either be left unmodified or will name a different symlink in a
 * symlink chain that started with the original path.
 */
static void resolve_symlink(struct strbuf *path)
{
	int depth = MAXDEPTH;
	static struct strbuf link = STRBUF_INIT;

	while (depth--) {
		if (strbuf_readlink(&link, path->buf, path->len) < 0)
			break;

		if (is_absolute_path(link.buf))
			/* absolute path simply replaces p */
			strbuf_reset(path);
		else
			/*
			 * link is a relative path, so replace the
			 * last element of p with it.
			 */
			trim_last_path_elm(path);

		strbuf_addbuf(path, &link);
	}
	strbuf_reset(&link);
}

static int lock_file(struct lock_file *lk, const char *path, int flags)
{
	if (!lock_file_list) {
		/* One-time initialization */
		sigchain_push_common(remove_lock_file_on_signal);
		atexit(remove_lock_file);
	}

	assert(!lk->active);

	if (!lk->on_list) {
		/* Initialize *lk and add it to lock_file_list: */
		lk->fd = -1;
		lk->active = 0;
		lk->owner = 0;
		lk->on_list = 1;
		strbuf_init(&lk->filename, 0);
		lk->next = lock_file_list;
		lock_file_list = lk;
	}

	strbuf_addstr(&lk->filename, path);
	if (!(flags & LOCK_NODEREF))
		resolve_symlink(&lk->filename);
	strbuf_addstr(&lk->filename, LOCK_SUFFIX);
	lk->fd = open(lk->filename.buf, O_RDWR | O_CREAT | O_EXCL, 0666);
	if (lk->fd < 0) {
		strbuf_reset(&lk->filename);
		return -1;
	}
	lk->owner = getpid();
	lk->active = 1;
	if (adjust_shared_perm(lk->filename.buf)) {
		error("cannot fix permission bits on %s", lk->filename.buf);
		rollback_lock_file(lk);
		return -1;
	}
	return lk->fd;
}

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

int hold_lock_file_for_update(struct lock_file *lk, const char *path, int flags)
{
	int fd = lock_file(lk, path, flags);
	if (fd < 0 && (flags & LOCK_DIE_ON_ERROR))
		unable_to_lock_die(path, errno);
	return fd;
}

int hold_lock_file_for_append(struct lock_file *lk, const char *path, int flags)
{
	int fd, orig_fd;

	fd = lock_file(lk, path, flags);
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
			rollback_lock_file(lk);
			return error("cannot open '%s' for copying", path);
		}
	} else if (copy_fd(orig_fd, fd)) {
		if (flags & LOCK_DIE_ON_ERROR)
			exit(128);
		rollback_lock_file(lk);
		return -1;
	}
	return fd;
}

int close_lock_file(struct lock_file *lk)
{
	int fd = lk->fd;
	lk->fd = -1;
	return close(fd);
}

int commit_lock_file(struct lock_file *lk)
{
	static struct strbuf result_file = STRBUF_INIT;
	int err;

	if (lk->fd >= 0 && close_lock_file(lk))
		return -1;

	if (!lk->active)
		die("BUG: attempt to commit unlocked object");

	/* remove ".lock": */
	strbuf_add(&result_file, lk->filename.buf,
		   lk->filename.len - LOCK_SUFFIX_LEN);
	err = rename(lk->filename.buf, result_file.buf);
	strbuf_reset(&result_file);
	if (err)
		return -1;
	lk->active = 0;
	strbuf_reset(&lk->filename);
	return 0;
}

int hold_locked_index(struct lock_file *lk, int die_on_error)
{
	return hold_lock_file_for_update(lk, get_index_file(),
					 die_on_error
					 ? LOCK_DIE_ON_ERROR
					 : 0);
}

void rollback_lock_file(struct lock_file *lk)
{
	if (lk->active) {
		if (lk->fd >= 0)
			close_lock_file(lk);
		unlink_or_warn(lk->filename.buf);
		lk->active = 0;
		strbuf_reset(&lk->filename);
	}
}
