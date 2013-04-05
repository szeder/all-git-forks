/*
 * Copyright (c) 2013 Ramkumar Ramachandra
 */
#include "cache.h"
#include "tree.h"
#include "cache-tree.h"
#include "unpack-trees.h"
#include "commit.h"
#include "diff.h"
#include "revision.h"

static int cat_file(struct cache_entry **src, struct unpack_trees_options *o) {
	int cached, match_missing = 1;
	unsigned dirty_submodule = 0;
	unsigned int mode;
	const unsigned char *sha1;
	struct cache_entry *idx = src[0];
	struct cache_entry *tree = src[1];
	struct rev_info *revs = o->unpack_data;
	enum object_type type;
	unsigned long size;
	char *buf;

	cached = o->index_only;
	if (ce_path_match(idx ? idx : tree, &revs->prune_data)) {
		if (get_stat_data(idx, &sha1, &mode, cached, match_missing,
					&dirty_submodule, NULL) < 0)
			die("Something went wrong!");
		buf = read_sha1_file(sha1, &type, &size);
		printf("%s", buf);
	}
	return 0;
}

int cmd_cat_link(int argc, const char **argv, const char *prefix)
{
	struct unpack_trees_options opts;
	int cached = 1;
	struct rev_info revs;
	struct tree *tree;
	struct tree_desc tree_desc;
	struct object_array_entry *ent;

	if (argc < 2)
		die("Usage: git cat-link <link>");

	init_revisions(&revs, prefix);
	setup_revisions(argc, argv, &revs, NULL); /* For revs.prune_data */
	add_head_to_pending(&revs);

	/* Hack to diff against index; we create a dummy tree for the
	   index information */
	if (!revs.pending.nr) {
		struct tree *tree;
		tree = lookup_tree(EMPTY_TREE_SHA1_BIN);
		add_pending_object(&revs, &tree->object, "HEAD");
	}

	if (read_cache() < 0)
		die("read_cache() failed");
	ent = revs.pending.objects;
	tree = parse_tree_indirect(ent->item->sha1);
	if (!tree)
		return error("bad tree object %s",
			     ent->name ? ent->name : sha1_to_hex(ent->item->sha1));

	memset(&opts, 0, sizeof(opts));
	opts.head_idx = 1;
	opts.index_only = cached;
	opts.diff_index_cached = cached;
	opts.merge = 1;
	opts.fn = cat_file;
	opts.unpack_data = &revs;
	opts.src_index = &the_index;
	opts.dst_index = NULL;
	opts.pathspec = &revs.diffopt.pathspec;
	opts.pathspec->recursive = 1;
	opts.pathspec->max_depth = -1;

	init_tree_desc(&tree_desc, tree->buffer, tree->size);
	unpack_trees(1, &tree_desc, &opts);
	return 0;
}
