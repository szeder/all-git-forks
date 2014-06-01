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
	struct strbuf destination;
};

extern int temp_file(struct temp_file *tf, const char *path, int flags);
extern void rollback_temp_file(struct temp_file *);
extern int commit_temp_file(struct temp_file *);
extern int close_temp_file(struct temp_file *);

#endif /* TEMPFILE_H */
