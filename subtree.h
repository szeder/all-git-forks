#ifndef SUBTREE_H
#define SUBTREE_H

#include "commit.h"
#include "string-list.h"

struct commit_list *get_subtrees(struct commit *commit, struct string_list *prefix_list, int exact);

#endif /* SUBTREE_H */