#ifndef SHM_H
#define SHM_H

/*
 * Create or open a shared memory and mmap it. Return mmap size if
 * successful, -1 otherwise. If successful mmap contains the mmap'd
 * pointer. If oflag does not contain O_CREAT and length is negative,
 * the mmap size is retrieved from existing shared memory object.
 *
 * The mmap could be freed by munmap, even on Windows. Note that on
 * Windows, git_shm_unlink() is no-op, so the last unmap will destroy
 * the shared memory.
 */
ssize_t git_shm_map(int oflag, int perm, ssize_t length, void **mmap,
		    int prot, int flags, const char *fmt, ...);

/*
 * Unlink a shared memory object. Only needed on POSIX platforms. On
 * Windows this is no-op.
 */
void git_shm_unlink(const char *fmt, ...);

#endif
