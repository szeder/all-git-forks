/*
 * Copyright (c) 2005, Junio C Hamano
 */
#include "cache.h"
#include "sigchain.h"

static struct lock_file *lock_file_list;

static void clear_filename(struct lock_file *lk)
{
	free(lk->filename);
	lk->filename = NULL;
}

static void remove_lock_file(void)
{
	pid_t me = getpid();

	while (lock_file_list) {
		if (lock_file_list->owner == me &&
		    lock_file_list->filename) {
			if (lock_file_list->fd >= 0)
				close(lock_file_list->fd);
			unlink_or_warn(lock_file_list->filename);
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
 * p = absolute or relative path name
 *
 * Return a pointer into p showing the beginning of the last path name
 * element.  If p is empty or the root directory ("/"), just return p.
 */
static char *last_path_elm(char *p)
{
	/* r starts pointing to null at the end of the string */
	char *r = strchr(p, '\0');

	if (r == p)
		return p; /* just return empty string */

	r--; /* back up to last non-null character */

	/* back up past trailing slashes, if any */
	while (r > p && *r == '/')
		r--;

	/*
	 * then go backwards until I hit a slash, or the beginning of
	 * the string
	 */
	while (r > p && *(r-1) != '/')
		r--;
	return r;
}


/* We allow "recursive" symbolic links. Only within reason, though */
#define MAXDEPTH 5

/*
 * p = path that may be a symlink
 * s = full size of p
 *
 * If p is a symlink, attempt to overwrite p with a path to the real
 * file or directory (which may or may not exist), following a chain of
 * symlinks if necessary.  Otherwise, leave p unmodified.
 *
 * This is a best-effort routine.  If an error occurs, p will either be
 * left unmodified or will name a different symlink in a symlink chain
 * that started with p's initial contents.
 *
 * Always returns p.
 */

static char *resolve_symlink(const char *in)
{
	static struct strbuf p = STRBUF_INIT;
	struct strbuf link = STRBUF_INIT;
	int depth = MAXDEPTH;

	strbuf_reset(&p);
	strbuf_addstr(&p, in);

	while (depth--) {
		if (strbuf_readlink(&link, p.buf, 0) < 0)
			break;	/* not a symlink anymore */

		if (is_absolute_path(link.buf)) {
			/* absolute path simply replaces p */
			strbuf_reset(&p);
			strbuf_addbuf(&p, &link);
		} else {
			/*
			 * link is a relative path, so I must replace the
			 * last element of p with it.
			 */
			char *r = (char *)last_path_elm(p.buf);
			strbuf_setlen(&p, r - p.buf);
			strbuf_addbuf(&p, &link);
		}
	}
	strbuf_release(&link);
	return p.buf;
}

/* Make sure errno contains a meaningful value on error */
static int lock_file(struct lock_file *lk, const char *path, int flags)
{
	struct strbuf sb = STRBUF_INIT;

	if (!(flags & LOCK_NODEREF) && !(path = resolve_symlink(path)))
		return -1;
	strbuf_add_absolute_path(&sb, path);
	strbuf_addstr(&sb, ".lock");
	lk->filename = strbuf_detach(&sb, NULL);
	lk->fd = open(lk->filename, O_RDWR | O_CREAT | O_EXCL, 0666);
	if (0 <= lk->fd) {
		if (!lock_file_list) {
			sigchain_push_common(remove_lock_file_on_signal);
			atexit(remove_lock_file);
		}
		lk->owner = getpid();
		if (!lk->on_list) {
			lk->next = lock_file_list;
			lock_file_list = lk;
			lk->on_list = 1;
		}
		if (adjust_shared_perm(lk->filename)) {
			int save_errno = errno;
			error("cannot fix permission bits on %s",
			      lk->filename);
			errno = save_errno;
			return -1;
		}
	}
	else
		clear_filename(lk);
	return lk->fd;
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

int unable_to_lock_error(const char *path, int err)
{
	struct strbuf buf = STRBUF_INIT;

	unable_to_lock_message(path, err, &buf);
	error("%s", buf.buf);
	strbuf_release(&buf);
	return -1;
}

NORETURN void unable_to_lock_index_die(const char *path, int err)
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
			int save_errno = errno;
			if (flags & LOCK_DIE_ON_ERROR)
				die("cannot open '%s' for copying", path);
			close(fd);
			error("cannot open '%s' for copying", path);
			errno = save_errno;
			return -1;
		}
	} else if (copy_fd(orig_fd, fd)) {
		int save_errno = errno;
		if (flags & LOCK_DIE_ON_ERROR)
			exit(128);
		close(fd);
		errno = save_errno;
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

int reopen_lock_file(struct lock_file *lk)
{
	if (0 <= lk->fd)
		die(_("BUG: reopen a lockfile that is still open"));
	if (!lk->filename[0])
		die(_("BUG: reopen a lockfile that has been committed"));
	lk->fd = open(lk->filename, O_WRONLY);
	return lk->fd;
}

int commit_lock_file(struct lock_file *lk)
{
	char *result_file;
	if ((lk->fd >= 0 && close_lock_file(lk)) || !lk->filename)
		return -1;
	result_file = xmemdupz(lk->filename,
			       strlen(lk->filename) - 5 /* .lock */);
	if (rename(lk->filename, result_file)) {
		free(result_file);
		return -1;
	}
	free(result_file);
	clear_filename(lk);
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
	if (lk->filename) {
		if (lk->fd >= 0)
			close(lk->fd);
		unlink_or_warn(lk->filename);
	}
	clear_filename(lk);
}
