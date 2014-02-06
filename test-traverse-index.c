#include <assert.h>
#include "git-compat-util.h"
#include "cache.h"
#include "string-list.h"
#include "tree-iter.h"
#include "ntree-iter.h"

static const char *usage_msg = "test-traverse-index";

struct tree_entry_list {
	struct tree_entry *entry;
	int nr;
	int alloc;
};

static void ntree_iter_verify(struct ntree_iter *iter,
		const struct tree_entry_list *expected)
{
	int pos;

	for (pos = 0; pos < expected->nr; pos++) {
		int i;

		assert(!ntree_iter_read_entry(iter));
		for (i = 0; i < iter->len; i++) {
			if (is_null_sha1(expected->entry[pos].sha1))
				assert(is_null_sha1(iter->entry[i].sha1));
			else
				assert(!tree_entry_cmp(&iter->entry[i], &expected->entry[pos]));
		}
	}

	assert(ntree_iter_read_entry(iter));
}

static void test_traverse_tree(struct tree_iter *iter,
		const struct tree_entry_list *sample)
{
	int i;
	for (i = 0; i < sample->nr; i++) {
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

	for (i = 0; i < sample->nr; i++) {
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

	for (i = 0; i < sample->nr; i++) {
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
	struct ntree_iter iter;
	struct strbuf treebuf = STRBUF_INIT;
	struct index_state *index;

	ntree_iter_init(&iter, 2);

	create_tree(&treebuf, sample);
	tree_iter_init_tree(&iter.tree[0], treebuf.buf, treebuf.len);

	index = create_index(sample);
	tree_iter_init_index(&iter.tree[1], index);

	ntree_iter_verify(&iter, sample);

	ntree_iter_release(&iter);
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
	entry->path = xstrdup(spec->path);
	entry->sha1 = sha1_from_obj_nr(spec->obj_nr);
}

static void tree_entry_list_append(struct tree_entry_list *list, struct tree_entry entry)
{
	ALLOC_GROW(list->entry, list->nr + 1, list->alloc);
	list->entry[list->nr++] = entry;
}

static void tree_entry_list_init(struct tree_entry_list *list, struct tree_entry_spec *spec)
{
	int i;

	list->nr = 0;
	while (spec[list->nr].path)
		list->nr++;
	list->entry = xmalloc(list->nr * sizeof(*list->entry));
	list->alloc = list->nr;
	for (i = 0; i < list->nr; i++)
		tree_entry_init(&list->entry[i], &spec[i]);
}

static void tree_entry_list_release(struct tree_entry_list *list)
{
	int i;

	for (i = 0; i < list->nr; i++) {
		free((char *)list->entry[i].path);
		free((char *)list->entry[i].sha1);
	}
	free(list->entry);
	memset(list, 0, sizeof(*list));
}

/* Remove trailing whitespace */
static int string_rstrip(struct string_list_item *item, void *cb_data)
{
	const char *whitespace = cb_data ? cb_data : " \n\t";
	char *s = item->string;
	char *first = NULL;

	while (*s) {
		const char *is_whitespace = strchr(whitespace, *s);
		if (!first && is_whitespace)
			first = s;
		else if (first && !is_whitespace)
			first = NULL;
		s++;
	}

	if (first)
		*first = '\0';

	return 0;
}

static const char *parse_tree_entry_spec(struct string_list *list, const char *line)
{
	const char *path;

	string_list_split(list, line, ' ', -1);
	string_list_remove_empty_items(list, 0);
	if (list->nr == 0)
		return NULL;

	path = xstrdup(list->items[0].string);
	string_list_delete_item(list, 0, 0);
	if (list->nr > 0)
		string_rstrip(&list->items[list->nr - 1], "\n");

	return path;
}

struct tree_list {
	struct tree_entry_list *entry;
	int nr, alloc;
};

static void tree_list_release(struct tree_list *list)
{
	int i;
	for (i = 0; i < list->nr; i++)
		tree_entry_list_release(&list->entry[i]);
	free(list->entry);
	memset(list, 0, sizeof(*list));
}

static void tree_list_add_path(struct tree_list *ntree_list, const char *line)
{
	int i;
	struct string_list entry_list = STRING_LIST_INIT_DUP;
	const char *path;

	path = parse_tree_entry_spec(&entry_list, line);
	if (!path)
		die("parse_tree_entry_spec failed");

	if (entry_list.nr > ntree_list->nr) {
		ALLOC_GROW(ntree_list->entry, entry_list.nr, ntree_list->alloc);
		ntree_list->nr = entry_list.nr;
	}

	for (i = 0; i < entry_list.nr; i++) {
		struct tree_entry entry;
		unsigned int obj_nr;
		char *endptr;
		const char *obj_nr_str = entry_list.items[i].string;

		if (!strcmp(obj_nr_str, "-"))
			continue;

		errno = 0;
		obj_nr = strtol(obj_nr_str, &endptr, 10);
		if (endptr == obj_nr_str || *endptr != '\0' || obj_nr < 0 || errno == ERANGE)
			die("invalid object nr: '%s'", obj_nr_str);

		entry.path = xstrdup(path);
		entry.sha1 = sha1_from_obj_nr(obj_nr);
		tree_entry_list_append(&ntree_list->entry[i], entry);
	}

	free((char *)path);
	string_list_clear(&entry_list, 0);
}

static struct tree_list tree_list_init(const char **ntree_spec)
{
	const char *line;
	struct tree_list ntree_list = { 0 };

	while ((line = *ntree_spec++))
		tree_list_add_path(&ntree_list, line);

	return ntree_list;
}

static void all_tests(void)
{
	int i, j;
	struct tree_entry_list sample;
	struct tree_entry_spec sample_spec[] = {
		{ 1, "a"},
		{ 2, "b"},
		{ 3, "c"},
		{ 0, NULL }
	};
	const char *ntree_spec[] = {
		"a - 1\n",
		"b 2 3\n",
		"c 4 -\n",
		"d 5 5\n",
		NULL
	};

	i = 0;
	const char *line;
	while ((line = ntree_spec[i])) {
		int j;
		const char *path;
		struct string_list list = STRING_LIST_INIT_DUP;

		path = parse_tree_entry_spec(&list, line);
		if (!path)
			die("parse_tree_entry_spec failed");

		printf("path: %s\n", path);
		for (j = 0; j < list.nr; j++)
			printf("%s, ", list.items[j].string);
		printf("\n");

		string_list_clear(&list, 0);
		i++;
	}

	struct tree_list ntree_list = tree_list_init(ntree_spec);
	for (i = 0; i < ntree_list.nr; i++) {
		printf("tree %i: ", i);
		for (j = 0; j < ntree_list.entry[i].nr; j++)
			printf("%s, ", ntree_list.entry[i].entry[j].path);
		printf("\n");
	}
	tree_list_release(&ntree_list);

	tree_entry_list_init(&sample, sample_spec);

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
