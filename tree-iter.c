#include "cache.h"
#include "tree-walk.h"
#include "tree-iter.h"

void tree_entry_setnull(struct tree_entry *entry)
{
	entry->path = NULL;
	entry->sha1 = null_sha1;
}

static int strnullcmp(const char *a, const char *b)
{
	if (a == b)
		return 0;
	else if (a == NULL)
		return -1;
	else if (b == NULL)
		return 1;
	else
		return strcmp(a, b);
}

int tree_entry_cmp(const struct tree_entry *a, const struct tree_entry *b)
{
	return (a->sha1 != b->sha1 && hashcmp(a->sha1, b->sha1)) ||
		strnullcmp(a->path, b->path);
}

/*
 * This is safe to call even if the iteration has ended and has no effect in
 * that case.
 */
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
	if (!desc->size) {
		tree_entry_setnull(entry);
		return;
	}

	entry->path = desc->entry.path;
	entry->sha1 = desc->entry.sha1;
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

struct index_desc {
	const struct index_state *index;
	int pos;
};

static void tree_entry_init_from_index_desc(struct tree_entry *entry,
		struct index_desc *desc)
{
	struct cache_entry *ce;

	if (!(desc->pos < desc->index->cache_nr)) {
		tree_entry_setnull(entry);
		return;
	}

	ce = desc->index->cache[desc->pos];
	entry->path = ce->name;
	entry->sha1 = ce->sha1;
}

static void tree_iter_next_index(struct tree_iter *iter)
{
	struct index_desc *desc = iter->cb_data;

	if (desc->pos < desc->index->cache_nr)
		desc->pos++;
	tree_entry_init_from_index_desc(&iter->entry, iter->cb_data);
}

void tree_iter_init_index(struct tree_iter *iter, const struct index_state *index)
{
	struct index_desc *desc = xcalloc(1, sizeof(*desc));

	desc->index = index;
	tree_entry_init_from_index_desc(&iter->entry, desc);

	iter->next = tree_iter_next_index;
	iter->cb_data = desc;
}
