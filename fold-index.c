#include "cache.h"
#include "cache-tree.h"
#include "unpack-trees.h"

static void *init_tree_desc_from_sha1(const unsigned char *sha1,
				      struct tree_desc *desc)
{
	enum object_type type;
	unsigned long size;
	void *buffer;

	buffer = read_sha1_file(sha1, &type, &size);
	if (!buffer || type != OBJ_TREE) {
		free(buffer);
		error("%s is unavailable or not a tree", sha1_to_hex(sha1));
		return NULL;
	}
	init_tree_desc(desc, buffer, size);
	return buffer;
}

static int cache_entries_to_tree(struct index_state *istate,
				 int start,
				 const char *path,
				 unsigned char *sha1)
{
	struct index_state temp = {0};
	int len = strlen(path);

	for (; start < istate->cache_nr; start++) {
		struct cache_entry *ce = istate->cache[start];

		if (ce_namelen(ce) <= len + 1 ||
		    ce->name[len] != '/' ||
		    !memcmp(path, ce->name, len))
			break;

		add_index_entry(&temp, dup_cache_entry(ce), ADD_CACHE_JUST_APPEND);
	}
	if (cache_tree_update(&temp, WRITE_TREE_SILENT) < 0) {
		discard_index(&temp);
		return error("failed to create tree %s", path);
	}
	hashcpy(sha1, temp.cache_tree->sha1);
	discard_index(&temp);
	return 0;
}

int fold_index(struct index_state *istate, const char *path)
{
	struct unpack_trees_options opts = {0};
	struct tree_desc tree_desc;
	unsigned char sha1[20];
	void *buffer;
	int pos, len;

	len = strlen(path);
	pos = index_dir_pos(istate, path, len);
	if (pos >= 0)
		return 0;

	if (cache_entries_to_tree(istate, -pos-1, path, sha1) < 0)
		return -1;

	buffer = init_tree_desc_from_sha1(sha1, &tree_desc);
	if (!buffer)
		return -1;

	memset(&opts, 0, sizeof(opts));
	opts.head_idx  = -1;
	opts.reset     = 0;
	opts.merge     = 1;
	opts.update    = 1;
	opts.fn        = subtract_merge;
	opts.src_index = istate;
	opts.dst_index = istate;
	if (unpack_trees(1, &tree_desc, &opts)) {
		free(buffer);
		return error("failed to fold %s", path);
	}
	free(buffer);
	return 0;
}
