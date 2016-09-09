#include "cache.h"
#include "diff.h"
#include "commit.h"
#include "sha1-lookup.h"
#include "patch-ids.h"

int commit_patch_id(struct commit *commit, struct diff_options *options,
		    unsigned char *sha1, int diff_header_only)
{
	if (commit->parents) {
		if (commit->parents->next)
			return PATCH_ID_NONE;
		diff_tree_sha1(commit->parents->item->object.oid.hash,
			       commit->object.oid.hash, "", options);
	} else
		diff_root_tree_sha1(commit->object.oid.hash, "", options);
	diffcore_std(options);
	if (diff_flush_patch_id(options, sha1, diff_header_only))
		return PATCH_ID_ERROR;
	return PATCH_ID_OK;
}

/* avoid repeating ourselves in patch_id_cmp */
static int cmp_setup(struct patch_id *p, struct diff_options *opt)
{
	if (!is_null_sha1(p->patch_id))
		return 0; /* OK, already computed id */

	switch (commit_patch_id(p->commit, opt, p->patch_id, 0)) {
	case PATCH_ID_OK:
		return 0;
	case PATCH_ID_ERROR:
		return error("Could not get patch ID for %s",
			     oid_to_hex(&p->commit->object.oid));
	case PATCH_ID_NONE:
		return -1; /* not an error, but nothing to compare */
	}
	die("BUG: unhandled patch_result");
}

/*
 * When we cannot load the full patch-id for both commits for whatever
 * reason, the function returns -1. Despite
 * the "cmp" in the name of this function, the caller only cares about
 * the return value being zero (a and b are equivalent) or non-zero (a
 * and b are different), and returning non-zero would keep both in the
 * result, even if they actually were equivalent, in order to err on
 * the side of safety.  The actual value being negative does not have
 * any significance; only that it is non-zero matters.
 */
static int patch_id_cmp(struct patch_id *a,
			struct patch_id *b,
			struct diff_options *opt)
{
	if (cmp_setup(a, opt) || cmp_setup(b, opt))
		return -1;
	return hashcmp(a->patch_id, b->patch_id);
}

int init_patch_ids(struct patch_ids *ids)
{
	memset(ids, 0, sizeof(*ids));
	diff_setup(&ids->diffopts);
	ids->diffopts.detect_rename = 0;
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
