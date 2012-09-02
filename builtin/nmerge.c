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

	write_to_index(&result);

	return 0;
}
