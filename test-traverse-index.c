#include "git-compat-util.h"
#include "cache.h"

static const char *usage_msg = "test-traverse-index";

struct tree_entry {
	unsigned int obj_nr;
	const char *path;
};

#define DIV_CEIL(num, den) (((num)+(den)-1)/(den))

static const unsigned char *sha1_from_obj_nr(unsigned int obj_nr)
{
	int i;
	static unsigned char sha1[20];
	unsigned int bitwidth = bitsizeof(sha1[0]);

	for (i = 0; i < DIV_CEIL(sizeof(obj_nr), sizeof(sha1[0])); i++)
		sha1[i] = (obj_nr >> (i * bitwidth)) & ~(1 << bitwidth);
	for (; i < ARRAY_SIZE(sha1); i++)
		sha1[i] = 0;

	return sha1;
}

static struct cache_entry *create_cache_entry(const struct tree_entry *entry)
{
	int mode = 0100644;
	struct cache_entry *ce;
	int namelen = strlen(entry->path);
	unsigned size = cache_entry_size(namelen);

	ce = xcalloc(1, size);
	hashcpy(ce->sha1, sha1_from_obj_nr(entry->obj_nr));
	memcpy(ce->name, entry->path, namelen);
	ce->ce_mode = create_ce_mode(mode);
	ce->ce_flags = create_ce_flags(0);
	ce->ce_namelen = namelen;

	return ce;
}

static void test_index(void)
{
	int i;
	struct index_state *index;
	struct tree_entry sample[] = {
		{ 1, "a"},
		{ 2, "c"},
		{ 3, "b"}
	};

	index = xcalloc(1, sizeof(*index));

	for (i = 0; i < ARRAY_SIZE(sample); i++) {
		struct cache_entry *ce;
		ce = create_cache_entry(&sample[i]);
		if (add_index_entry(index, ce, ADD_CACHE_OK_TO_ADD) < 0)
			die(_("unable to add cache entry: %s"), ce->name);
	}

	discard_index(index);
	free(index);
}

int main(int argc, char **argv)
{
	if (argc > 1)
		usage(usage_msg);

	test_index();

	return 0;
}
