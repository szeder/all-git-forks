#include "tree-iter.h"
#include "ntree-iter.h"

/*
 * Returns non-zero if iteration has ended. The returned entry is invalidated
 * by ntree_iter_next.
 */
int ntree_iter_read_entry(struct tree_iter *iter, int n_trees,
		struct tree_entry **entry)
{
	int i;
	const char *first;

	first = NULL;
	for (i = 0; i < n_trees; i++) {
		if (!first) {
			first = iter[i].entry.path;
			continue;
		}

	}
	if (!first)
		return 1;

	for (i = 0; i < n_trees; i++) {
		if (!strcmp(iter[i].entry.path, first))
			entry[i] = &iter[i].entry;
		else
			entry[i] = NULL;
	}

	return 0;
}

void ntree_iter_next(struct tree_iter *iter, int n_trees)
{
	int i;

	for (i = 0; i < n_trees; i++)
		tree_iter_next(&iter[i]);
}

void ntree_iter_release(struct tree_iter *iter, int n_trees)
{
	int i;

	for (i = 0; i < n_trees; i++)
		tree_iter_release(&iter[i]);
}
