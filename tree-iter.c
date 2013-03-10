#include "cache.h"
#include "tree-walk.h"
#include "tree-iter.h"

void tree_iter_next(struct tree_iter *iter)
{
	iter->next(iter);
}

int tree_iter_eof(const struct tree_iter *iter)
{
	return !iter->entry.path;
}

void tree_iter_release(struct tree_iter *iter)
{
	free(iter->cb_data);
	iter->cb_data = NULL;
}

static void tree_entry_init_from_tree_desc(struct tree_entry *entry,
		struct tree_desc *desc)
{
	if (desc->size) {
		entry->path = desc->entry.path;
		hashcpy(entry->sha1, desc->entry.sha1);
	} else {
		entry->path = NULL;
		hashcpy(entry->sha1, null_sha1);
	}
}

static void tree_iter_next_tree(struct tree_iter *iter)
{
	struct tree_desc *desc = iter->cb_data;

	update_tree_entry(desc);
	tree_entry_init_from_tree_desc(&iter->entry, desc);
}

void tree_iter_init_tree(struct tree_iter *iter, char *buffer, int size)
{
	struct tree_desc *desc = xmalloc(sizeof(*desc));

	init_tree_desc(desc, buffer, size);
	tree_entry_init_from_tree_desc(&iter->entry, desc);
	iter->next = tree_iter_next_tree;
	iter->cb_data = desc;
}
