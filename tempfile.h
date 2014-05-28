#ifndef TEMPFILE_H
#define TEMPFILE_H

#include "git-compat-util.h"
#include "strbuf.h"

struct temp_file {
    struct temp_file *volatile next;
    volatile sig_atomic_t active;
    volatile int fd;
    volatile pid_t owner;
    char on_list;
    struct strbuf filename;
};


extern int initialize_temp_file(struct temp_file *tmp, const char *path, int flags);

/*
 * "Commits" a temporary file by renaming it to dest.buf, then "deactivating"
 * the file (thereby ensuring it is not unlinked atexit(3)). The temp_file
 * pointer may not be used after this operation; its memory will be freed
 * atexit(3).
 */
extern int rename_tempfile_into_place(struct temp_file *tmp, const char *dest);

/*
 * "Rolls back" a temporary file. If the file is still open,
 * this function closes it. It also removes the file from the
 * filesystem. The file may not be used after this operation;
 * it will be removed atexit(3).
 */
extern void rollback_temp_file(struct temp_file *tmp);

/*
 * Closes a temporary file. The file is closed and the its
 * file descriptor fd is set to -1. The file may not be used after this
 * operation; it will be removed atexit(3).
 */
extern int close_temp_file(struct temp_file *tmp);

#endif /* TEMPFILE_H */
