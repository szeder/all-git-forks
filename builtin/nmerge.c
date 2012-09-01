#include "git-compat-util.h"
#include "cache.h"
#include "tree-walk.h"
#include "tree.h"
#include "unpack-trees.h"

static const char * const read_tree_usage[] = {
	"git nmerge <treeish>",
	NULL
};

int cmd_nmerge(int argc, const char **argv, const char *unused_prefix)
{
	struct lock_file lock_file;
	int newfd;
	const char *dst_head = argv[1];
	unsigned char sha1[20];
	struct tree *tree;
	struct tree_desc tree_desc;
	struct unpack_trees_options opts;
	struct traverse_info info;

	if (argc != 2)
		die("Wrong number of arguments");

	if (get_sha1(dst_head, sha1))
		die("Not a valid object name %s", dst_head);
	tree = parse_tree_indirect(sha1);
	if (!tree)
		die("Failed to unpack tree object %s", dst_head);

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

	the_index = opts.result;
	newfd = hold_locked_index(&lock_file, 1);
	if (write_cache(newfd, active_cache, active_nr))
		die("write_cache: unable to write new index file");
	if (commit_locked_index(&lock_file))
		die("commit_locked_index: unable to write new index file");

	return 0;
}
