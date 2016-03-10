#ifndef REBASE_MERGE_H
#define REBASE_MERGE_H
#include "rebase-common.h"

const char *git_path_rebase_merge_dir(void);

/*
 * The rebase_merge backend is a merge-based non-interactive mode that copes
 * well with renamed files.
 */
struct rebase_merge {
	struct rebase_options opts;
	char *dir;
	unsigned int msgnum, end;
	int prec;
};

void rebase_merge_init(struct rebase_merge *, const char *dir);

void rebase_merge_release(struct rebase_merge *);

int rebase_merge_in_progress(const struct rebase_merge *);

int rebase_merge_load(struct rebase_merge *);

void rebase_merge_run(struct rebase_merge *);

#endif /* REBASE_MERGE_H */
