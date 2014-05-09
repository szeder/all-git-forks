#include "git-compat-util.h"
#include "shm.h"

#ifdef HAVE_SHM

#define SHM_PATH_LEN 33		/* Limit is from OS X */

ssize_t git_shm_map(int oflag, int perm, ssize_t length, void **mmap,
		    int prot, int flags, const char *fmt, ...)
{
	va_list ap;
	char path[SHM_PATH_LEN];
	int fd;
	ssize_t requested_length = length;
	off_t reported_length = length;
	ssize_t *base_mmap;

	length += sizeof(ssize_t);

	path[0] = '/';
	va_start(ap, fmt);
	vsprintf(path + 1, fmt, ap);
	va_end(ap);
	fd = shm_open(path, oflag, perm);
	if (fd < 0)
		return -1;
	if (requested_length > 0 && ftruncate(fd, length)) {
		shm_unlink(path);
		close(fd);
		return -1;
	}
	if (requested_length < 0 && !(oflag & O_CREAT)) {
		struct stat st;
		if (fstat(fd, &st))
			die_errno("unable to stat %s", path);
		reported_length = st.st_size;
	}
	base_mmap = xmmap(NULL, reported_length, prot, flags, fd, 0);
	close(fd);
	if (base_mmap == MAP_FAILED) {
		*mmap = NULL;
		shm_unlink(path);
		return -1;
	}
	*mmap = base_mmap + 1;
	if (requested_length > 0)
		*base_mmap = requested_length;
	return *base_mmap;
}

void git_shm_unlink(const char *fmt, ...)
{
	va_list ap;
	char path[SHM_PATH_LEN];

	path[0] = '/';
	va_start(ap, fmt);
	vsprintf(path + 1, fmt, ap);
	va_end(ap);
	shm_unlink(path);
}

#else

ssize_t git_shm_map(int oflag, int perm, ssize_t length, void **mmap,
		    int prot, int flags, const char *fmt, ...)
{
	return -1;
}

void git_shm_unlink(const char *fmt, ...)
{
}

#endif
