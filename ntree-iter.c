#include "cache.h"
#include "tree-iter.h"
#include "ntree-iter.h"

void ntree_iter_init(struct ntree_iter *iter, int len)
{
	int i;

	iter->tree = xcalloc(len, sizeof(*iter->tree));
	iter->entry = xmalloc(len * sizeof(*iter->entry));
	for (i = 0; i < len; i++)
		tree_entry_setnull(&iter->entry[i]);
	iter->len = len;
}

/*
 * Returns non-zero if iteration has ended.
 */
void ntree_iter_next(struct ntree_iter *iter)
{
	int i;

	for (i = 0; i < iter->len; i++)
		tree_iter_next(&iter->tree[i]);
}

int ntree_iter_read_entry(struct ntree_iter *iter)
{
	int i;
	const char *first;

	first = NULL;
	for (i = 0; i < iter->len; i++) {
		if (!first) {
			first = iter->tree[i].entry.path;
			continue;
		}

	}
	if (!first)
		return 1;

	for (i = 0; i < iter->len; i++) {
		if (!strcmp(iter->tree[i].entry.path, first))
			iter->entry[i] = iter->tree[i].entry;
		else
			tree_entry_setnull(&iter->entry[i]);
	}

	ntree_iter_next(iter);

	return 0;
}

void ntree_iter_release(struct ntree_iter *iter)
{
	int i;

	for (i = 0; i < iter->len; i++)
		tree_iter_release(&iter->tree[i]);
}
