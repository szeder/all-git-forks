#include "cache.h"

int copy_fd(int ifd, int ofd, struct strbuf *err)
{
	assert(err);

	while (1) {
		char buffer[8192];
		ssize_t len = xread(ifd, buffer, sizeof(buffer));
		if (!len)
			break;
		if (len < 0) {
			strbuf_addf(err, "read returned %s", strerror(errno));
			return -1;
		}
		if (write_in_full(ofd, buffer, len) < 0) {
			strbuf_addf(err, "write returned %s", strerror(errno));
			return -1;
		}
	}
	return 0;
}

static int copy_times(const char *dst, const char *src)
{
	struct stat st;
	struct utimbuf times;
	if (stat(src, &st) < 0)
		return -1;
	times.actime = st.st_atime;
	times.modtime = st.st_mtime;
	if (utime(dst, &times) < 0)
		return -1;
	return 0;
}

int copy_file(const char *dst, const char *src, int mode)
{
	int fdi, fdo;
	struct strbuf err = STRBUF_INIT;

	mode = (mode & 0111) ? 0777 : 0666;
	if ((fdi = open(src, O_RDONLY)) < 0)
		return fdi;
	if ((fdo = open(dst, O_WRONLY | O_CREAT | O_EXCL, mode)) < 0) {
		close(fdi);
		return fdo;
	}
	if (copy_fd(fdi, fdo, &err)) {
		close(fdi);
		close(fdo);
		error("copy-fd: %s", err.buf);
		strbuf_release(&err);
		return -1;
	}
	strbuf_release(&err);
	close(fdi);
	if (close(fdo) != 0)
		return error("%s: close error: %s", dst, strerror(errno));
	if (adjust_shared_perm(dst))
		return -1;

	return 0;
}

int copy_file_with_time(const char *dst, const char *src, int mode)
{
	int status = copy_file(dst, src, mode);
	if (!status)
		return copy_times(dst, src);
	return status;
}
