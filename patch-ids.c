#include "cache.h"
#include "diff.h"
#include "commit.h"
#include "sha1-lookup.h"
#include "patch-ids.h"

int commit_patch_id(struct commit *commit, struct diff_options *options,
		    unsigned char *sha1, int diff_header_only)
{
	if (commit->parents)
		diff_tree_sha1(commit->parents->item->object.oid.hash,
			       commit->object.oid.hash, "", options);
	else
		diff_root_tree_sha1(commit->object.oid.hash, "", options);
	diffcore_std(options);
	return diff_flush_patch_id(options, sha1, diff_header_only);
}

static int patch_id_cmp(struct patch_id *a,
			struct patch_id *b,
			struct diff_options *opt)
{
	if (is_null_sha1(a->patch_id) &&
	    commit_patch_id(a->commit, opt, a->patch_id, 0))
		return error("Could not get patch ID for %s",
			oid_to_hex(&a->commit->object.oid));
	if (is_null_sha1(b->patch_id) &&
	    commit_patch_id(b->commit, opt, b->patch_id, 0))
		return error("Could not get patch ID for %s",
			oid_to_hex(&b->commit->object.oid));
	return hashcmp(a->patch_id, b->patch_id);
}

int init_patch_ids(struct patch_ids *ids)
{
	memset(ids, 0, sizeof(*ids));
	diff_setup(&ids->diffopts);
	DIFF_OPT_SET(&ids->diffopts, RECURSIVE);
	diff_setup_done(&ids->diffopts);
	hashmap_init(&ids->patches, (hashmap_cmp_fn)patch_id_cmp, 256);
	return 0;
}

int free_patch_ids(struct patch_ids *ids)
{
	hashmap_free(&ids->patches, 1);
	return 0;
}

static int init_patch_id_entry(struct patch_id *patch,
			       struct commit *commit,
			       struct patch_ids *ids)
{
	unsigned char header_only_patch_id[GIT_SHA1_RAWSZ];

	patch->commit = commit;
	if (commit_patch_id(commit, &ids->diffopts, header_only_patch_id, 1))
		return -1;

	hashmap_entry_init(patch, sha1hash(header_only_patch_id));
	return 0;
}

struct patch_id *has_commit_patch_id(struct commit *commit,
				     struct patch_ids *ids)
{
	struct patch_id patch;

	memset(&patch, 0, sizeof(patch));
	if (init_patch_id_entry(&patch, commit, ids))
		return NULL;

	return hashmap_get(&ids->patches, &patch, &ids->diffopts);
}

struct patch_id *add_commit_patch_id(struct commit *commit,
				     struct patch_ids *ids)
{
	struct patch_id *key = xcalloc(1, sizeof(*key));

	if (init_patch_id_entry(key, commit, ids)) {
		free(key);
		return NULL;
	}

	hashmap_add(&ids->patches, key);
	return key;
}
