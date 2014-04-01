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
 * When the first file is locked, it registers an atexit(3) handler;
 * when the program exits, the handler rolls back any files that have
 * been locked but were never committed or rolled back.
 *
 * A lock_file object can be in several states:
 *
 * - Uninitialized.  In this state the object's flags field must be
 *   zero but the rest of the contents need not be initialized.  In
 *   particular, the lock_filename strbuf should *not* be initialized
 *   externally.  The first time the object is used in any way, it is
 *   initialized, permanently registered in the lock_file_list, and
 *   flags & LOCK_FLAGS_ON_LIST is set.
 *
 * - Locked, lockfile open (after hold_lock_file_for_update() or
 *   hold_lock_file_for_append()).  In this state, the lockfile
 *   exists, lock_filename holds the filename of the lockfile, fd
 *   holds a file descriptor open for writing to the lockfile, and
 *   owner holds the PID of the process that locked the file.
 *
 * - Locked, lockfile closed (after close_lock_file()).  Same as the
 *   previous state, except that the lockfile is closed and fd is -1.
 *
 * - Unlocked (after commit_lock_file(), rollback_lock_file(), or a
 *   failed attempt to lock).  In this state, lock_filename is the
 *   empty string and fd is -1.  The object is left registered in the
 *   lock_file_list, and flags & LOCK_FLAGS_ON_LIST is set.
 *
 * See Documentation/api-lockfile.txt for more information.
 */

/* Values for lock_file::flags: */

/* This lock_file instance is in the lock_file_list */
#define LOCK_FLAGS_ON_LIST 0x01

static struct lock_file *lock_file_list;
static const char *alternate_index_output;

static void remove_lock_file(void)
{
	pid_t me = getpid();

	while (lock_file_list) {
		if (lock_file_list->owner == me &&
		    lock_file_list->lock_filename.len) {
			if (lock_file_list->fd >= 0)
				close(lock_file_list->fd);
			unlink_or_warn(lock_file_list->lock_filename.buf);
		}
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
 * path contains a path that may be a symlink
 *
 * If path is a symlink, attempt to overwrite it with a path to the
 * real file or directory (which may or may not exist), following a
 * chain of symlinks if necessary.  Otherwise, leave path unmodified.
 *
 * This is a best-effort routine.  If an error occurs, path will
 * either be left unmodified or will name a different symlink in a
 * symlink chain that started with path's initial contents.
 */
static void resolve_symlink(struct strbuf *path)
{
	int depth = MAXDEPTH;
	struct strbuf link;

	strbuf_init(&link, path->len);

	while (depth--) {
		if (strbuf_readlink(&link, path->buf, path->len) < 0)
			break;

		if (is_absolute_path(link.buf))
			/* an absolute path replaces the whole path: */
			strbuf_setlen(path, 0);
		else
			/* a relative path replaces the last element of path: */
			trim_last_path_elm(path);

		strbuf_addbuf(path, &link);
	}

	strbuf_release(&link);
}

/* We append ".lock" to the filename to derive the lockfile name: */
#define LOCK_SUFFIX_LEN 5

static int lock_file(struct lock_file *lk, const char *path, int flags)
{
	size_t path_len = strlen(path);

	if (!lock_file_list) {
		/* One-time initialization */
		sigchain_push_common(remove_lock_file_on_signal);
		atexit(remove_lock_file);
	}

	lk->owner = getpid();
	if (lk->flags & LOCK_FLAGS_ON_LIST) {
		assert(!lk->lock_filename.len); /* object not already in use */
		if (strbuf_avail(&lk->lock_filename) < path_len + LOCK_SUFFIX_LEN)
			strbuf_grow(&lk->lock_filename, path_len + LOCK_SUFFIX_LEN);
	} else {
		/* Initialize *lk and add it to lock_file_list: */
		lk->fd = -1;
		lk->flags |= LOCK_FLAGS_ON_LIST;
		strbuf_init(&lk->lock_filename, path_len + LOCK_SUFFIX_LEN);
		lk->next = lock_file_list;
		lock_file_list = lk;
	}

	strbuf_add(&lk->lock_filename, path, path_len);
	if (!(flags & LOCK_NODEREF))
		resolve_symlink(&lk->lock_filename);
	strbuf_addstr(&lk->lock_filename, ".lock");

	lk->fd = open(lk->lock_filename.buf, O_RDWR | O_CREAT | O_EXCL, 0666);
	if (0 <= lk->fd) {
		if (adjust_shared_perm(lk->lock_filename.buf)) {
			error("cannot fix permission bits on %s", lk->lock_filename.buf);
			rollback_lock_file(lk);
			return -1;
		}
	}
	else
		strbuf_setlen(&lk->lock_filename, 0);
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

NORETURN void unable_to_lock_index_die(const char *path, int err)
{
	die("%s", unable_to_lock_message(path, err));
}

int hold_lock_file_for_update(struct lock_file *lk, const char *path, int flags)
{
	int fd = lock_file(lk, path, flags);
	if (fd < 0 && (flags & LOCK_DIE_ON_ERROR))
		unable_to_lock_index_die(path, errno);
	return fd;
}

int hold_lock_file_for_append(struct lock_file *lk, const char *path, int flags)
{
	int fd, orig_fd;

	fd = lock_file(lk, path, flags);
	if (fd < 0) {
		if (flags & LOCK_DIE_ON_ERROR)
			unable_to_lock_index_die(path, errno);
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
	char *result_file;
	size_t path_len = lk->lock_filename.len - LOCK_SUFFIX_LEN;
	int err = 0;

	if (lk->fd >= 0 && close_lock_file(lk))
		return -1;
	result_file = xmemdupz(lk->lock_filename.buf, path_len);
	if (rename(lk->lock_filename.buf, result_file))
		err = -1;
	else
		strbuf_setlen(&lk->lock_filename, 0);
	free(result_file);
	return err;
}

int hold_locked_index(struct lock_file *lk, int die_on_error)
{
	return hold_lock_file_for_update(lk, get_index_file(),
					 die_on_error
					 ? LOCK_DIE_ON_ERROR
					 : 0);
}

void set_alternate_index_output(const char *name)
{
	alternate_index_output = name;
}

int commit_locked_index(struct lock_file *lk)
{
	if (alternate_index_output) {
		if (lk->fd >= 0 && close_lock_file(lk))
			return -1;
		if (rename(lk->lock_filename.buf, alternate_index_output))
			return -1;
		strbuf_setlen(&lk->lock_filename, 0);
		return 0;
	}
	else
		return commit_lock_file(lk);
}

void rollback_lock_file(struct lock_file *lk)
{
	if (lk->lock_filename.len) {
		if (lk->fd >= 0)
			close_lock_file(lk);
		unlink_or_warn(lk->lock_filename.buf);
		strbuf_setlen(&lk->lock_filename, 0);
	}
}
