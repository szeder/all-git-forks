#include "git-compat-util.h"
#include "shm.h"

#ifdef HAVE_SHM

#define SHM_PATH_LEN 72		/* we don't create very long paths.. */

ssize_t git_shm_map(int oflag, int perm, ssize_t length, void **mmap,
		    int prot, int flags, const char *fmt, ...)
{
	va_list ap;
	char path[SHM_PATH_LEN];
	int fd;

	path[0] = '/';
	va_start(ap, fmt);
	vsprintf(path + 1, fmt, ap);
	va_end(ap);
	fd = shm_open(path, oflag, perm);
	if (fd < 0)
		return -1;
	if (length > 0 && ftruncate(fd, length)) {
		shm_unlink(path);
		close(fd);
		return -1;
	}
	if (length < 0 && !(oflag & O_CREAT)) {
		struct stat st;
		if (fstat(fd, &st))
			die_errno("unable to stat %s", path);
		length = st.st_size;
	}
	*mmap = xmmap(NULL, length, prot, flags, fd, 0);
	close(fd);
	if (*mmap == MAP_FAILED) {
		*mmap = NULL;
		shm_unlink(path);
		return -1;
	}
	return length;
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
