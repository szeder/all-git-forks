#include <assert.h>
#include "git-compat-util.h"
#include "cache.h"
#include "tree-walk.h"

static const char *usage_msg = "test-traverse-index";

struct tree_entry {
	unsigned int obj_nr;
	const char *path;
	unsigned char sha1[20];
};

static const unsigned char *sha1_from_obj_nr(unsigned int obj_nr)
{
	int i;
	static unsigned char sha1[20];

	for (i = 0; i < ARRAY_SIZE(sha1); i++)
		sha1[i] = 0;
	for (i = 0; i < sizeof(obj_nr); i++)
		sha1[i] = (obj_nr >> (i * CHAR_BIT)) & ~(1 << CHAR_BIT);

	return sha1;
}

static void tree_entry_init(struct tree_entry *entry)
{
	hashcpy(entry->sha1, sha1_from_obj_nr(entry->obj_nr));
}

#define DIV_CEIL(num, den) (((num)+(den)-1)/(den))

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

static struct index_state *create_index(struct tree_entry *sample, int n_samples)
{
	int i;
	struct index_state *index;

	index = xcalloc(1, sizeof(*index));

	for (i = 0; i < n_samples; i++) {
		struct cache_entry *ce;
		ce = create_cache_entry(&sample[i]);
		if (add_index_entry(index, ce, ADD_CACHE_OK_TO_ADD) < 0)
			die(_("unable to add cache entry: %s"), ce->name);
	}

	return index;
}

static void test_index(struct tree_entry *sample, int n_samples)
{
	struct index_state *index;

	index = create_index(sample, n_samples);

	discard_index(index);
	free(index);
}

static void create_tree(struct strbuf *treebuf, struct tree_entry *sample, int n_samples)
{
	int i;
	unsigned int mode = 0100644;

	for (i = 0; i < n_samples; i++) {
		struct tree_entry *entry = &sample[i];
		strbuf_addf(treebuf, "%o %s%c", mode, entry->path, '\0');
		strbuf_add(treebuf, entry->sha1, 20);
	}
}

#define OBJ_NR_SIZE sizeof(((struct tree_entry *)NULL)->obj_nr)

static void test_traverse_tree(struct tree_entry *sample, int n_samples,
		char *buffer, int len)
{
	int i;
	struct tree_desc desc;

	init_tree_desc(&desc, buffer, len);
	for (i = 0; i < n_samples; i++) {
		assert(desc.size);
		assert(!hashcmp(desc.entry.sha1, sample[i].sha1));
		assert(!strcmp(desc.entry.path, sample[i].path));

		update_tree_entry(&desc);
	}

	/* end of tree */
	assert(!desc.size);
}

static void test_tree(struct tree_entry *sample, int n_samples)
{
	struct strbuf treebuf = STRBUF_INIT;

	create_tree(&treebuf, sample, n_samples);
	test_traverse_tree(sample, n_samples, treebuf.buf, treebuf.len);

	strbuf_release(&treebuf);
}

static void all_tests(void)
{
	int i;

	struct tree_entry sample[] = {
		{ 1, "a"},
		{ 2, "c"},
		{ 3, "b"}
	};

	for (i = 0; i < ARRAY_SIZE(sample); i++)
		tree_entry_init(&sample[i]);

	test_index(sample, ARRAY_SIZE(sample));
	test_tree(sample, ARRAY_SIZE(sample));
}

int main(int argc, char **argv)
{
	if (argc > 1)
		usage(usage_msg);

	all_tests();

	return 0;
}
