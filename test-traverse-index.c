#include <assert.h>
#include "git-compat-util.h"
#include "cache.h"
#include "tree-iter.h"
#include "ntree-iter.h"

static const char *usage_msg = "test-traverse-index";

struct tree_entry_list {
	struct tree_entry *entry;
	int len;
};

static void ntree_iter_verify(struct tree_iter *iter, int n_trees,
		const struct tree_entry_list *expected)
{
	int pos;
	struct tree_entry *entry = xcalloc(n_trees, sizeof(*entry));

	for (pos = 0; pos < expected->len; pos++) {
		int i;

		assert(!ntree_iter_read_entry(iter, n_trees, entry));
		for (i = 0; i < n_trees; i++)
			assert(!tree_entry_cmp(&entry[i], &expected->entry[pos]));

		ntree_iter_next(iter, n_trees);
	}
	assert(ntree_iter_read_entry(iter, n_trees, entry));

	free(entry);
}

static void test_traverse_tree(struct tree_iter *iter,
		const struct tree_entry_list *sample)
{
	int i;
	for (i = 0; i < sample->len; i++) {
		struct tree_entry *entry = &sample->entry[i];

		assert(iter->entry.path);
		assert(!tree_entry_cmp(&iter->entry, entry));

		tree_iter_next(iter);
	}

	/* end of tree */
	assert(tree_iter_eof(iter));
}

static struct cache_entry *create_cache_entry(const struct tree_entry *entry)
{
	int mode = 0100644;
	struct cache_entry *ce;
	int namelen = strlen(entry->path);
	unsigned size = cache_entry_size(namelen);

	ce = xcalloc(1, size);
	hashcpy(ce->sha1, entry->sha1);
	memcpy(ce->name, entry->path, namelen);
	ce->ce_mode = create_ce_mode(mode);
	ce->ce_flags = create_ce_flags(0);
	ce->ce_namelen = namelen;

	return ce;
}

static struct index_state *create_index(const struct tree_entry_list *sample)
{
	int i;
	struct index_state *index;

	index = xcalloc(1, sizeof(*index));

	for (i = 0; i < sample->len; i++) {
		struct cache_entry *ce;
		ce = create_cache_entry(&sample->entry[i]);
		if (add_index_entry(index, ce, ADD_CACHE_OK_TO_ADD) < 0)
			die(_("unable to add cache entry: %s"), ce->name);
	}

	return index;
}

static void test_index(const struct tree_entry_list *sample)
{
	struct index_state *index;
	struct tree_iter iter;

	index = create_index(sample);

	tree_iter_init_index(&iter, index);
	test_traverse_tree(&iter, sample);

	tree_iter_release(&iter);
	discard_index(index);
	free(index);
}

static void create_tree(struct strbuf *treebuf, const struct tree_entry_list *sample)
{
	int i;
	unsigned int mode = 0100644;

	for (i = 0; i < sample->len; i++) {
		struct tree_entry *entry = &sample->entry[i];
		strbuf_addf(treebuf, "%o %s%c", mode, entry->path, '\0');
		strbuf_add(treebuf, entry->sha1, 20);
	}
}

static void test_tree(const struct tree_entry_list *sample)
{
	struct strbuf treebuf = STRBUF_INIT;
	struct tree_iter iter;

	create_tree(&treebuf, sample);

	tree_iter_init_tree(&iter, treebuf.buf, treebuf.len);
	test_traverse_tree(&iter, sample);

	tree_iter_release(&iter);
	strbuf_release(&treebuf);
}

static void test_ntree(const struct tree_entry_list *sample)
{
	struct tree_iter iter[2];
	struct strbuf treebuf = STRBUF_INIT;
	struct index_state *index;

	memset(iter, 0, sizeof(iter));

	create_tree(&treebuf, sample);
	tree_iter_init_tree(&iter[0], treebuf.buf, treebuf.len);

	index = create_index(sample);
	tree_iter_init_index(&iter[1], index);

	ntree_iter_verify(iter, ARRAY_SIZE(iter), sample);

	ntree_iter_release(iter, ARRAY_SIZE(iter));
	strbuf_release(&treebuf);
	discard_index(index);
	free(index);
}

struct tree_entry_spec {
	unsigned int obj_nr;
	const char *path;
};

static const unsigned char *sha1_from_obj_nr(unsigned int obj_nr)
{
	int i;
	unsigned char *sha1 = xmalloc(20 * sizeof(*sha1));

	hashcpy(sha1, null_sha1);
	for (i = 0; i < sizeof(obj_nr); i++)
		sha1[i] = (obj_nr >> (i * CHAR_BIT)) & ~(1 << CHAR_BIT);

	return sha1;
}

static void tree_entry_init(struct tree_entry *entry, struct tree_entry_spec *spec)
{
	entry->path = spec->path;
	entry->sha1 = sha1_from_obj_nr(spec->obj_nr);
}

static void tree_entry_list_init(struct tree_entry_list *list, struct tree_entry_spec *spec, int len)
{
	int i;

	list->len = len;
	list->entry = xmalloc(len * sizeof(*list->entry));
	for (i = 0; i < len; i++)
		tree_entry_init(&list->entry[i], &spec[i]);
}

static void tree_entry_list_release(struct tree_entry_list *list)
{
	int i;

	for (i = 0; i < list->len; i++)
		free((char *)list->entry[i].sha1);
	free(list->entry);
	list->entry = NULL;
	list->len = 0;
}

static void all_tests(void)
{
	struct tree_entry_list sample;
	struct tree_entry_spec sample_spec[] = {
		{ 1, "a"},
		{ 2, "b"},
		{ 3, "c"}
	};

	tree_entry_list_init(&sample, sample_spec, ARRAY_SIZE(sample_spec));

	test_index(&sample);
	test_tree(&sample);
	test_ntree(&sample);

	tree_entry_list_release(&sample);
}

int main(int argc, char **argv)
{
	if (argc > 1)
		usage(usage_msg);

	all_tests();

	return 0;
}
