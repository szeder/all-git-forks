#ifndef SHM_H
#define SHM_H

/*
 * Create or open a shared memory and mmap it. Return mmap size if
 * successful, -1 otherwise. If successful mmap contains the mmap'd
 * pointer. If oflag does not contain O_CREAT and length is negative,
 * the mmap size is retrieved from existing shared memory object.
 *
 * On some platforms, ftruncate on a shm always silently rounds up to
 * a page size boundary.  In order to work around this, the shared
 * memory created will be sizeof(size_t) longer than requested.  The
 * extra bytes are used to hold the originally requested length so
 * that later shm_open.  The returned pointer will point to the memory
 * *after* the length data, and the returned size will exclude that
 * data.
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
