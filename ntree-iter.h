#ifndef NTREE_ITER_H
#define NTREE_ITER_H

#include "tree-iter.h"

struct ntree_iter {
	struct tree_iter *tree;
	struct tree_entry *entry;
	int len;
};

void ntree_iter_init(struct ntree_iter *iter, int len);
void ntree_iter_next(struct ntree_iter *iter);
int ntree_iter_read_entry(struct ntree_iter *iter);
void ntree_iter_release(struct ntree_iter *iter);

#endif /* NTREE_ITER_H */
