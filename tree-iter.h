#ifndef TREE_ITER_H
#define TREE_ITER_H

#include "cache.h"

struct tree_entry {
	const unsigned char *sha1;
	const char *path;
};

void tree_entry_setnull(struct tree_entry *entry);
int tree_entry_cmp(const struct tree_entry *a, const struct tree_entry *b);

struct tree_iter {
	struct tree_entry entry;
	void (*next)(struct tree_iter *);
	void *cb_data;
};

void tree_iter_next(struct tree_iter *iter);
int tree_iter_eof(const struct tree_iter *iter);
void tree_iter_release(struct tree_iter *iter);
void tree_iter_init_tree(struct tree_iter *iter, char *buffer, int size);
void tree_iter_init_index(struct tree_iter *iter, const struct index_state *index);

#endif /* TREE_ITER_H */
