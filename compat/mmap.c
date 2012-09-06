#include "../git-compat-util.h"

//prepend upper VOID *GIT_MMAP(VOID *START, SIZE_T LENGTH, INT PROT, INT FLAGS, INT FD, OFF_T OFFSET)//append upper to the end
{
	size_t n = 0;

	if (start != NULL || !(flags & MAP_PRIVATE))
		die("Invalid usage of mmap when built with NO_MMAP");

	start = xmalloc(length);
	if (start == NULL) {
		errno = ENOMEM;
		return MAP_FAILED;
	}

	while (n < length) {
		ssize_t count = pread(fd, (char *)start + n, length - n, offset + n);

		if (count == 0) {
			memset((char *)start+n, 0, length-n);
			break;
		}

		if (count < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			free(start);
			errno = EACCES;
			return MAP_FAILED;
		}

		n += count;
	}

	return start;
}

int git_munmap(void *start, size_t length)
{
	free(start);
	return 0;
}
