#ifndef PICK_H
#define PICK_H

#include "commit.h"

/* Pick flags: */
#define PICK_REVERSE   1 /* pick the reverse changes ("revert") */
#define PICK_ADD_NOTE  2 /* add note about original commit (unless conflict) */
/* We don't need a PICK_QUIET. This is done by
 *	setenv("GIT_MERGE_VERBOSITY", "0", 1); */
extern int pick_commit(struct commit *commit, int mainline, int flags, struct strbuf *msg);

#endif
