#include "cache.h"
#include "diff.h"
#include "diffcore.h"
#include "commit.h"
#include "sha1-lookup.h"
#include "patch-ids.h"

static int commit_patch_id(struct commit *commit, struct diff_options *options,
		    unsigned char *sha1)
{
	if (commit->parents)
		diff_tree_sha1(commit->parents->item->object.sha1,
		               commit->object.sha1, "", options);
	else
		diff_root_tree_sha1(commit->object.sha1, "", options);
	diffcore_std(options);
	return diff_flush_patch_id(options, sha1);
}

struct collect_paths_info {
	/* The (sorted) list of paths touched by recorded commits. */
	struct string_list *paths;
	/* Are we recording paths or checking for ones we haven't seen? */
	int searching;
};

/*
 * Compare two trees (or one tree with the empty tree) and record paths
 * changed between the trees in data.  If data->searching is true then
 * instead of recording the paths, return a negative result if any of the
 * changed paths is not present in data->paths.
 */
static int changed_paths_recursive(int n, struct tree_desc *t,
	const char *base, struct pathspec *pathspec,
	struct collect_paths_info *data);

static int same_entry(struct name_entry *a, struct name_entry *b)
{
	if (!a->sha1 || !b->sha1)
		return a->sha1 == b->sha1;
	return !hashcmp(a->sha1, b->sha1) && a->mode == b->mode;
}

static int process_changed_path(struct collect_paths_info *info, const char *path)
{
	if (info->searching) {
		if (!string_list_has_string(info->paths, path))
			return -1;
	} else
		string_list_insert(info->paths, path);
	return 0;
}

static inline const unsigned char *dir_sha1(struct name_entry *e)
{
	if (S_ISDIR(e->mode))
		return e->sha1;
	return NULL;
}

static int changed_paths_cb(int n, unsigned long mask,
		unsigned long dirmask, struct name_entry *entry,
		struct traverse_info *info)
{
	struct collect_paths_info *collect_info = info->data;
	if (n == 1) {
		/* We're handling a root commit - add all the paths. */
		if (entry[0].sha1 && !S_ISDIR(entry[0].mode)) {
			if (process_changed_path(collect_info, entry[0].path))
				return -1;
		} else if (S_ISDIR(entry[0].mode)) {
			char *newbase = alloc_traverse_path(info, entry);
			struct tree_desc t[1];
			void *buf0 = fill_tree_descriptor(t, entry[0].sha1);
			int error = changed_paths_recursive(1, t, newbase,
					info->pathspec, collect_info);
			free(buf0);
			free(newbase);
			if (error < 0)
				return error;
		}
		return mask;
	}

	if (same_entry(entry+0, entry+1))
		return mask;

	if (entry[0].mode && !S_ISDIR(entry[0].mode))
		if (process_changed_path(collect_info, entry[0].path))
			return -1;
	if (entry[1].mode && !S_ISDIR(entry[1].mode))
		if (process_changed_path(collect_info, entry[1].path))
			return -1;

	if ((entry[0].sha1 && S_ISDIR(entry[0].mode)) ||
	    (entry[1].sha1 && S_ISDIR(entry[1].mode))) {
		char *newbase = alloc_traverse_path(info,
				S_ISDIR(entry[0].mode) ? entry+0 : entry+1);
		struct tree_desc t[2];
		void *buf0 = fill_tree_descriptor(t+0, dir_sha1(entry+0));
		void *buf1 = fill_tree_descriptor(t+1, dir_sha1(entry+1));
		int error = changed_paths_recursive(2, t, newbase,
				info->pathspec, collect_info);
		free(buf0);
		free(buf1);
		free(newbase);
		if (error < 0)
			return error;
	}

	return mask;
}

static int changed_paths_recursive(int n, struct tree_desc *t,
		const char *base, struct pathspec *pathspec,
		struct collect_paths_info *data)
{
	struct traverse_info info;

	setup_traverse_info(&info, base);
	info.data = data;
	info.fn = changed_paths_cb;
	info.pathspec = pathspec;

	return traverse_trees(n, t, &info);
}

/*
 * When searching is false, update ids->touched_paths with the paths changed
 * in the given commit.  When searching is true return a negative result if
 * the commit changes any paths not in ids->touched_paths.
 */
static int changed_paths(struct commit *commit, struct patch_ids *ids,
		int searching)
{
	struct tree_desc trees[2];
	struct collect_paths_info info = { &ids->touched_paths, searching };
	void *commitbuf;
	int result;

	commitbuf = fill_tree_descriptor(trees + 1, commit->object.sha1);
	if (commit->parents) {
		/*
		 * Like commit_patch_id above, we compare this commit against
		 * its first parent.
		 */
		void *parentbuf = fill_tree_descriptor(trees + 0,
					commit->parents->item->object.sha1);
		result = changed_paths_recursive(2, trees, "",
				&ids->diffopts.pathspec, &info);
		free(parentbuf);
	} else {
		result = changed_paths_recursive(1, trees + 1, "",
				&ids->diffopts.pathspec, &info);
	}
	free(commitbuf);
	return result;
}

static const unsigned char *patch_id_access(size_t index, void *table)
{
	struct patch_id **id_table = table;
	return id_table[index]->patch_id;
}

static int patch_pos(struct patch_id **table, int nr, const unsigned char *id)
{
	return sha1_pos(id, table, nr, patch_id_access);
}

#define BUCKET_SIZE 190 /* 190 * 21 = 3990, with slop close enough to 4K */
struct patch_id_bucket {
	struct patch_id_bucket *next;
	int nr;
	struct patch_id bucket[BUCKET_SIZE];
};

int init_patch_ids(struct patch_ids *ids)
{
	memset(ids, 0, sizeof(*ids));
	diff_setup(&ids->diffopts);
	DIFF_OPT_SET(&ids->diffopts, RECURSIVE);
	diff_setup_done(&ids->diffopts);
	ids->touched_paths.strdup_strings = 1;
	return 0;
}

int free_patch_ids(struct patch_ids *ids)
{
	struct patch_id_bucket *next, *patches;

	free(ids->table);
	for (patches = ids->patches; patches; patches = next) {
		next = patches->next;
		free(patches);
	}

	string_list_clear(&ids->touched_paths, 0);
	return 0;
}

static struct patch_id *add_commit(struct commit *commit,
				   struct patch_ids *ids,
				   int searching)
{
	struct patch_id_bucket *bucket;
	struct patch_id *ent;
	unsigned char sha1[20];
	int pos;

	if (!searching)
		changed_paths(commit, ids, 0);
	else if (changed_paths(commit, ids, 1) < 0)
		return NULL;

	if (commit_patch_id(commit, &ids->diffopts, sha1))
		return NULL;
	pos = patch_pos(ids->table, ids->nr, sha1);
	if (0 <= pos)
		return ids->table[pos];
	if (searching)
		return NULL;

	pos = -1 - pos;

	bucket = ids->patches;
	if (!bucket || (BUCKET_SIZE <= bucket->nr)) {
		bucket = xcalloc(1, sizeof(*bucket));
		bucket->next = ids->patches;
		ids->patches = bucket;
	}
	ent = &bucket->bucket[bucket->nr++];
	hashcpy(ent->patch_id, sha1);

	ALLOC_GROW(ids->table, ids->nr + 1, ids->alloc);
	if (pos < ids->nr)
		memmove(ids->table + pos + 1, ids->table + pos,
			sizeof(ent) * (ids->nr - pos));
	ids->nr++;
	ids->table[pos] = ent;
	return ids->table[pos];
}

struct patch_id *has_commit_patch_id(struct commit *commit,
				     struct patch_ids *ids)
{
	return add_commit(commit, ids, 1);
}

struct patch_id *add_commit_patch_id(struct commit *commit,
				     struct patch_ids *ids)
{
	return add_commit(commit, ids, 0);
}
