#include "git-compat-util.h"
#include "cache.h"
#include "tree-walk.h"
#include "tree.h"
#include "unpack-trees.h"

static const char * const read_tree_usage[] = {
	"git nmerge <treeish>",
	NULL
};

static int test_read_tree(struct index_state *result, const unsigned char *sha1)
{
	struct tree *tree;
	struct tree_desc tree_desc;
	struct unpack_trees_options opts;
	struct traverse_info info;

	tree = parse_tree_indirect(sha1);
	if (!tree)
		return 1;

	parse_tree(tree);
	init_tree_desc(&tree_desc, tree->buffer, tree->size);

	memset(&opts, 0, sizeof(opts));
	opts.result.initialized = 1;

	extern int unpack_callback(int n, unsigned long mask, unsigned long dirmask, struct name_entry *names, struct traverse_info *info);
	setup_traverse_info(&info, "");
	info.fn = unpack_callback;
	info.data = &opts;
	info.show_all_errors = 1;
	info.pathspec = opts.pathspec;

	if (traverse_trees(1, &tree_desc, &info) < 0)
		return 3;

	*result = opts.result;

	return 0;
}

static void write_to_index(struct index_state *result)
{
	struct lock_file lock_file;
	int newfd;

	newfd = hold_locked_index(&lock_file, 1);
	the_index = *result;
	if (write_cache(newfd, active_cache, active_nr))
		die("write_cache: unable to write new index file");
	if (commit_locked_index(&lock_file))
		die("commit_locked_index: unable to write new index file");
}

static int cmp_name(struct cache_entry *a, struct cache_entry *b)
{
	return cache_name_compare(a->name, a->ce_flags, b->name, b->ce_flags);
}

static struct cache_entry *next_entry_stage(struct index_state *index, int *i,
					    int stage)
{
	struct cache_entry *ce = NULL;

	while (*i < index->cache_nr) {
		ce = index->cache[*i];
		(*i)++;
		if (ce_stage(ce) == stage)
			break;
	}

	return ce;
}

void diff_index_index(struct index_state *index_a, struct index_state *index_b)
{
	int pos_a, pos_b;
	int cmp = 0;

	pos_a = 0;
	pos_b = 0;
	while (1) {
		struct cache_entry *a, *b;
		const char *name;
		const char *kind;

		if (cmp <= 0)
			a = next_entry_stage(index_a, &pos_a, 0);
		if (cmp >= 0)
			b = next_entry_stage(index_b, &pos_b, 0);
		if (a && b)
			cmp = cmp_name(a, b);
		else if (a && !b)
			cmp = -1;
		else if (!a && b)
			cmp = 1;
		else
			break;

		if (cmp == 0) {
			if (hashcmp(a->sha1, b->sha1))
				kind = "change";
			else
				kind = "same  ";
			name = a->name;
		} else if (cmp < 0) {
			kind = "rm    ";
			name = a->name;
		} else {
			kind = "add   ";
			name = b->name;
		}

		printf("%s %s\n", kind, name);
	}
}

int cmd_nmerge(int argc, const char **argv, const char *unused_prefix)
{
	const char *dst_head = argv[1];
	unsigned char sha1[20];
	struct index_state result;

	if (argc != 2)
		die("Wrong number of arguments");

	if (get_sha1(dst_head, sha1))
		die("Not a valid object name %s", dst_head);

	if (test_read_tree(&result, sha1))
		die("Failed to read tree from %s", dst_head);

	read_cache();
	diff_index_index(&the_index, &result);

	write_to_index(&result);

	return 0;
}
