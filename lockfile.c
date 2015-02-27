/*
 * Copyright (c) 2005, Junio C Hamano
 */
#include "cache.h"
#include "tempfile.h"
#include "lockfile.h"
#include "sigchain.h"

/*
 * path = absolute or relative path name
 *
 * Remove the last path name element from path (leaving the preceding
 * "/", if any).  If path is empty or the root directory ("/"), set
 * path to the empty string.
 */
static void trim_last_path_component(struct strbuf *path)
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
			trim_last_path_component(path);

		strbuf_addbuf(path, &link);
	}
	strbuf_reset(&link);
}

/* Make sure errno contains a meaningful value on error */
static int lock_file(struct lock_file *lk, const char *path, int flags)
{
	int fd;
	struct strbuf filename = STRBUF_INIT;

	strbuf_addstr(&filename, path);
	if (!(flags & LOCK_NO_DEREF))
		resolve_symlink(&filename);

	strbuf_addstr(&filename, LOCK_SUFFIX);
	fd = create_tempfile(&lk->tempfile, filename.buf);
	strbuf_reset(&filename);
	return fd;
}

void unable_to_lock_message(const char *path, int err, struct strbuf *buf)
{
	if (err == EEXIST) {
		strbuf_addf(buf, "Unable to create '%s.lock': %s.\n\n"
		    "If no other git process is currently running, this probably means a\n"
		    "git process crashed in this repository earlier. Make sure no other git\n"
		    "process is running and remove the file manually to continue.",
			    absolute_path(path), strerror(err));
	} else
		strbuf_addf(buf, "Unable to create '%s.lock': %s",
			    absolute_path(path), strerror(err));
}

NORETURN void unable_to_lock_die(const char *path, int err)
{
	struct strbuf buf = STRBUF_INIT;

	unable_to_lock_message(path, err, &buf);
	die("%s", buf.buf);
}

/* This should return a meaningful errno on failure */
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
			int save_errno = errno;

			if (flags & LOCK_DIE_ON_ERROR)
				die("cannot open '%s' for copying", path);
			rollback_lock_file(lk);
			error("cannot open '%s' for copying", path);
			errno = save_errno;
			return -1;
		}
	} else if (copy_fd(orig_fd, fd)) {
		int save_errno = errno;

		if (flags & LOCK_DIE_ON_ERROR)
			exit(128);
		close(orig_fd);
		rollback_lock_file(lk);
		errno = save_errno;
		return -1;
	} else {
		close(orig_fd);
	}
	return fd;
}

FILE *fdopen_lock_file(struct lock_file *lk, const char *mode)
{
	return fdopen_tempfile(&lk->tempfile, mode);
}

char *get_locked_file_path(struct lock_file *lk)
{
	if (!lk->tempfile.active)
		die("BUG: get_locked_file_path() called for unlocked object");
	if (lk->tempfile.filename.len <= LOCK_SUFFIX_LEN)
		die("BUG: get_locked_file_path() called for malformed lock object");
	return xmemdupz(lk->tempfile.filename.buf, lk->tempfile.filename.len - LOCK_SUFFIX_LEN);
}

int close_lock_file(struct lock_file *lk)
{
	return close_tempfile(&lk->tempfile);
}

int reopen_lock_file(struct lock_file *lk)
{
	return reopen_tempfile(&lk->tempfile);
}

int commit_lock_file_to(struct lock_file *lk, const char *path)
{
	return rename_tempfile(&lk->tempfile, path);
}

int commit_lock_file(struct lock_file *lk)
{
	static struct strbuf result_file = STRBUF_INIT;
	int err;

	if (!lk->tempfile.active)
		die("BUG: attempt to commit unlocked object");

	if (lk->tempfile.filename.len <= LOCK_SUFFIX_LEN ||
	    strcmp(lk->tempfile.filename.buf + lk->tempfile.filename.len - LOCK_SUFFIX_LEN,
		   LOCK_SUFFIX))
		die("BUG: lockfile filename corrupt");

	/* remove ".lock": */
	strbuf_add(&result_file, lk->tempfile.filename.buf,
		   lk->tempfile.filename.len - LOCK_SUFFIX_LEN);
	err = commit_lock_file_to(lk, result_file.buf);
	strbuf_reset(&result_file);
	return err;
}

void rollback_lock_file(struct lock_file *lk)
{
	delete_tempfile(&lk->tempfile);
}
