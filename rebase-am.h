#ifndef REBASE_AM_H
#define REBASE_AM_H
#include "rebase-common.h"

const char *git_path_rebase_am_dir(void);

struct rebase_am {
	struct rebase_options opts;
	char *dir;
};

int rebase_am_in_progress(const struct rebase_am *);

void rebase_am_init(struct rebase_am *, const char *dir);

void rebase_am_release(struct rebase_am *);

int rebase_am_load(struct rebase_am *);

void rebase_am_save(struct rebase_am *);

/**
 * Forcefully terminate the current am rebase.
 */
void rebase_am_destroy(struct rebase_am *);

void rebase_am_run(struct rebase_am *, const char **am_opts);

void rebase_am_continue(struct rebase_am *);

void rebase_am_skip(struct rebase_am *);

#endif /* REBASE_AM_H */
