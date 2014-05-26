#ifndef TEMPFILE_H
#define TEMPFILE_H

#include "cache.h"
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
extern void rollback_temp_file(struct temp_file *tmp);
extern int close_temp_file(struct temp_file *tmp);

#endif /* TEMPFILE_H */
