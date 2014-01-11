#ifndef NTREE_ITER_H
#define NTREE_ITER_H

#include "tree-iter.h"

void ntree_iter_next(struct tree_iter *iter, int n_trees);
int ntree_iter_read_entry(struct tree_iter *iter, int n_trees,
		struct tree_entry *entry);
void ntree_iter_release(struct tree_iter *iter, int n_trees);

#endif /* NTREE_ITER_H */
