#include "cache.h"
#include "safe-append.h"
#include "strbuf.h"
#include "lockfile.h"

static struct trace_key safeappend_trace = TRACE_KEY_INIT(SAFEAPPEND);

static int write_size_file(const char *path, off_t size)
{
	static struct lock_file size_lock;
	struct strbuf size_path = STRBUF_INIT;
	uint64_t size_buf;
	int lock_fd;

	strbuf_addstr(&size_path, path);
	strbuf_addstr(&size_path, ".size");
	lock_fd = hold_lock_file_for_update(&size_lock, size_path.buf,
					    LOCK_DIE_ON_ERROR);
	strbuf_release(&size_path);
	size_buf = htonll(size);
	write_or_die(lock_fd, &size_buf, 8);

	if (xfsync(lock_fd) < 0)
		die_errno("failed to fsync %s.size.lock", path);

	return commit_lock_file(&size_lock);
}

/*
 * Read the size file for a safeappend file and truncate the base file
 * if necessary.  If `create` is non-zero, the safeappend file is
 * being created, so if a size file should be created if it does not
 * yet exist.
 */
static int truncate_safeappend_file(const char *path, int create)
{
	struct strbuf size_path = STRBUF_INIT;
	uint64_t size;
	int need_size_file = 0;
	struct stat st;
	int size_fd;

	strbuf_addf(&size_path, "%s.size", path);
	size_fd = open(size_path.buf, O_RDONLY);
	strbuf_release(&size_path);

	if (size_fd > 0) {
		if (read_in_full(size_fd, &size, 8) != 8)
			die_errno("failed to read %s.size", path);

		size = ntohll(size);
		close(size_fd);
	} else {
		if (errno != ENOENT)
			die_errno("failed to read %s.size", path);

		/*
		 * If there is no size file, then we have never
		 * successfully appended to the file.  So we must
		 * treat the file as if it did not previously
		 * exist.
		 */
		if (!create) {
			errno = ENOENT;
			return -1;
		}
		size = 0;
		need_size_file = 1;
	}

	if (stat(path, &st) < 0) {
		if (size > 0)
			die_errno("failed to open file: size file exists, but %s is unreadable", path);
		if (create)
			/* it's OK: the file doesn't exist, so there's
			 * no need to truncate it */
			st.st_size = 0;
		else
			return -1;

	}

	if (st.st_size < size)
		die_errno("%s.size reports a too-large size for %s.",
			  path, path);

	if (st.st_size > size) {
		trace_printf_key(&safeappend_trace,
				 "Truncating %s to %"PRIu64" bytes",
				 path, size);
		if (truncate(path, size))
			die_errno("failed to truncate %s to saved size %"PRIu64,
				  path, size);
	} else {
		trace_printf_key(&safeappend_trace,
				 "No need to truncate %s to %"PRIu64" bytes",
				 path, size);
	}

	if (need_size_file && write_size_file(path, 0))
		return -1;

	return 0;
}

int open_safeappend_file(const char *path, int flags, int mode)
{
	int fd;

	if (truncate_safeappend_file(path, flags & O_CREAT) < 0)
		return -1;

	if (flags & O_CREAT) {
		trace_printf_key(&safeappend_trace, "Opening %s O_CREAT", path);
		fd = open(path, flags, mode);
		if (fd < 0)
			return fd;
		if (lseek(fd, 0, SEEK_END) < 0)
			die_errno("lseek on %s, flags %o, mode %o failed\n", path, flags, mode);
	} else {
		fd = open(path, flags);
	}

	return fd;
}

int stat_safeappend_file(const char *path, struct stat *st)
{

	if (truncate_safeappend_file(path, 0) < 0)
		return -1;

	return stat(path, st);
}

int lock_safeappend_file(struct safeappend_lock *ctx, const char *path,
			  int flags)
{
	struct stat st;

	if (hold_lock_file_for_update(&ctx->lock_file, path, flags) < 0)
		return -1;

	ctx->fd = open_safeappend_file(path, O_CREAT | O_RDWR, 0666);
	if (ctx->fd < 0)
		rollback_lock_file(&ctx->lock_file);

	if (fstat(ctx->fd, &st))
		die_errno("failed to fstat %s\n", path);
	ctx->original_size = xsize_t(st.st_size);
	return ctx->fd;
}

void rollback_locked_safeappend_file(struct safeappend_lock *ctx)
{
	if (ctx->fd < 0)
		return;

	rollback_lock_file(&ctx->lock_file);
	/*
	 * Since no new size file was ever written, the size file will
	 * contain the original size. So this ftruncate is permitted
	 * to fail, because we'll retry it when we next attempt to
	 * open the file.
	 */
	if (ftruncate(ctx->fd, ctx->original_size))
		warning("failed to truncate %s",
			  get_locked_file_path(&ctx->lock_file));
	close(ctx->fd);
	ctx->fd = -1;
}

int commit_safeappend_file(const char *path, int fd)
{
	struct stat st;

	if (fstat(fd, &st))
		die_errno("failed to fstat %s for commit", path);

	trace_printf_key(&safeappend_trace,"Committing %s at %"PRIuMAX" bytes",
			 path, (uintmax_t)st.st_size);

	if (xfsync(fd) < 0)
		die_errno("failed to fsync %s", path);
	close(fd);

	return write_size_file(path, st.st_size);
}

int commit_locked_safeappend_file(struct safeappend_lock *ctx)
{
	char *path;
	int ret;

	path = get_locked_file_path(&ctx->lock_file);
	ret = commit_safeappend_file(path, ctx->fd);
	ctx->fd = -1;
	free(path);

	return ret;
}

int unlink_safeappend_file(const char *path)
{
	struct strbuf pathbuf = STRBUF_INIT;
	int fd;
	char *dir;
	int ret = -1;

	/*
	 * To unlink a safeappend file, we first unlink the size file,
	 * which means that future attempts to open the file with
	 * open_safeappend_file will get ENOENT.
	 */

	strbuf_addstr(&pathbuf, path);
	strbuf_addstr(&pathbuf, ".size");

	if (unlink(pathbuf.buf))
		goto done;

	dir = dirname(pathbuf.buf);
	fd = open(dir, O_RDONLY);
	if (fd < 0)
		goto done;

	if (xfsync(fd) < 0)
		goto done;

	ret = unlink(path);
done:
	strbuf_release(&pathbuf);
	return ret;
}
