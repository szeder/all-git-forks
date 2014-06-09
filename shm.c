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

#elif defined(GIT_WINDOWS_NATIVE)

#define SHM_PATH_LEN 82	/* a little bit longer than POSIX because of "Local\\" */

static ssize_t create_shm_map(int oflag, int perm, ssize_t length,
			      void **mmap, int prot, int flags,
			      const char *path, unsigned long page_size)
{
	size_t real_length;
	void *last_page;
	HANDLE h;

	assert(perm   == 0700);
	assert(oflag  == (O_CREAT | O_EXCL | O_RDWR));
	assert(prot   == (PROT_READ | PROT_WRITE));
	assert(flags  == MAP_SHARED);
	assert(length >= 0);

	real_length = length;
	if (real_length % page_size)
		real_length += page_size - (real_length % page_size);
	real_length += page_size;
	h = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
			      real_length, path);
	if (!h)
		return -1;
	*mmap = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, real_length);
	CloseHandle(h);
	if (!*mmap)
		return -1;
	last_page = (unsigned char *)*mmap + real_length - page_size;
	*(unsigned long *)last_page = length;
	return length;
}

static ssize_t open_shm_map(int oflag, int perm, ssize_t length, void **mmap,
			    int prot, int flags, const char *path,
			    unsigned long page_size)
{
	void *last_page;
	HANDLE h;

	assert(perm   == 0700);
	assert(oflag  == O_RDONLY);
	assert(prot   == PROT_READ);
	assert(flags  == MAP_SHARED);
	assert(length <= 0);

	h = OpenFileMapping(FILE_MAP_READ, FALSE, path);
	if (!h)
		return -1;
	*mmap = MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
	CloseHandle(h);
	if (!*mmap)
		return -1;
	if (length < 0) {
		MEMORY_BASIC_INFORMATION mbi;
		if (!VirtualQuery(*mmap, &mbi, sizeof(mbi))) {
			UnmapViewOfFile(*mmap);
			return -1;
		}
		if (mbi.RegionSize % page_size)
			die("expected size %lu to be %lu aligned",
				    mbi.RegionSize, page_size);
		last_page = (unsigned char *)*mmap + mbi.RegionSize - page_size;
		length = *(unsigned long *)last_page;
	}
	return length;
}

ssize_t git_shm_map(int oflag, int perm, ssize_t length, void **mmap,
		    int prot, int flags, const char *fmt, ...)
{
	SYSTEM_INFO si;
	va_list ap;
	char path[SHM_PATH_LEN];

	GetSystemInfo(&si);

	strcpy(path, "Local\\");
	va_start(ap, fmt);
	vsprintf(path + strlen(path), fmt, ap);
	va_end(ap);

	if (oflag & O_CREAT)
		return create_shm_map(oflag, perm, length, mmap, prot,
				      flags, path, si.dwPageSize);
	else
		return open_shm_map(oflag, perm, length, mmap, prot,
				    flags, path, si.dwPageSize);
}

void git_shm_unlink(const char *fmt, ...)
{
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
