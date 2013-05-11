#include "cache.h"
#include "blob.h"
#include "diff.h"
#include "commit.h"
#include "notes.h"
#include "refs.h"
#include "sha1-lookup.h"
#include "patch-ids.h"
#include "version.h"

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

static int patch_id_config(const char *var, const char *value, void *cb)
{
	const char **cacheref = cb;

	if (!strcmp(var, "patchid.cacheref"))
		return git_config_string(cacheref, var, value);

	return 0;
}

int init_patch_ids(struct patch_ids *ids)
{
	memset(ids, 0, sizeof(*ids));
	diff_setup(&ids->diffopts);
	DIFF_OPT_SET(&ids->diffopts, RECURSIVE);
	diff_setup_done(&ids->diffopts);
	return 0;
}

static void sha1_update_str(git_SHA_CTX *ctx, const char *s)
{
	size_t len = s ? strlen(s) + 1 : 0;
	long nl = htonl((long) len);
	git_SHA1_Update(ctx, &nl, sizeof(nl));
	git_SHA1_Update(ctx, s, len);
}

static void sha1_update_int(git_SHA_CTX *ctx, int v)
{
	long nv = htonl((long) v);
	git_SHA1_Update(ctx, &nv, sizeof(nv));
}

static void sha1_update_pathspec(git_SHA_CTX *ctx, struct pathspec *pathspec)
{
	int i;
	/*
	 * Pathspecs are uniquely identified by their number and match string
	 * providing that we take limit_pathspec_to_literal into account.
	 */
	sha1_update_int(ctx, limit_pathspec_to_literal());
	sha1_update_int(ctx, pathspec->nr);
	for (i = 0; i < pathspec->nr; i++)
		sha1_update_str(ctx, pathspec->items[i].match);
}

static void hash_diff_options(struct diff_options *options, unsigned char *sha1)
{
	git_SHA_CTX ctx;
	git_SHA1_Init(&ctx);

	sha1_update_str(&ctx, options->filter);
	/* ignore options->orderfile (see setup_patch_ids) */
	sha1_update_str(&ctx, options->pickaxe);
	sha1_update_str(&ctx, options->single_follow);
	/* a_prefix and b_prefix aren't used for patch IDs */

	sha1_update_int(&ctx, options->flags);
	/* use_color isn't used for patch IDs */
	sha1_update_int(&ctx, options->context);
	sha1_update_int(&ctx, options->interhunkcontext);
	sha1_update_int(&ctx, options->break_opt);
	sha1_update_int(&ctx, options->detect_rename);
	sha1_update_int(&ctx, options->irreversible_delete);
	sha1_update_int(&ctx, options->skip_stat_unmatch);
	/* line_termination isn't used for patch IDs */
	/* output_format isn't used for patch IDs */
	sha1_update_int(&ctx, options->pickaxe_opts);
	sha1_update_int(&ctx, options->rename_score);
	sha1_update_int(&ctx, options->rename_limit);
	/* needed_rename_limit is set while diffing */
	/* degraded_cc_to_c is set while diffing */
	/* show_rename_progress isn't used for patch IDs */
	/* dirstat_permille isn't used for patch IDs */
	/* setup isn't used for patch IDs */
	/* abbrev isn't used for patch IDs */
	/* prefix and prefix_length aren't used for patch IDs */
	/* stat_sep isn't used for patch IDs */
	sha1_update_int(&ctx, options->xdl_opts);

	/* stat arguments aren't used for patch IDs */
	sha1_update_str(&ctx, options->word_regex);
	sha1_update_int(&ctx, options->word_diff);

	sha1_update_pathspec(&ctx, &options->pathspec);

	git_SHA1_Final(sha1, &ctx);
}

int setup_patch_ids(struct patch_ids *ids)
{
	char *cacheref = NULL;

	/*
	 * Make extra sure we aren't using an orderfile as it is unnecessary
	 * and will break caching.
	 */
	ids->diffopts.orderfile = NULL;

	git_config(patch_id_config, &cacheref);
	if (cacheref) {
		unsigned char diffopts_raw_hash[20];
		struct strbuf sb = STRBUF_INIT;
		strbuf_addstr(&sb, cacheref);
		expand_notes_ref(&sb);

		ids->cache = xcalloc(1, sizeof(*ids->cache));
		init_notes(ids->cache, sb.buf, combine_notes_overwrite, 0);

		hash_diff_options(&ids->diffopts, diffopts_raw_hash);
		strbuf_reset(&sb);
		strbuf_addf(&sb, "diffopts:%s\n", sha1_to_hex(diffopts_raw_hash));
		ids->diffopts_hash_len = sb.len;
		ids->diffopts_hash = strbuf_detach(&sb, NULL);

		free(cacheref);
	}

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

	if (ids->cache) {
		unsigned char notes_sha1[20];
		if (write_notes_tree(ids->cache, notes_sha1) ||
		    update_ref("patch-id: update cache", ids->cache->ref,
				notes_sha1, NULL, 0, QUIET_ON_ERR))
			error(_("failed to write patch ID cache"));

		free_notes(ids->cache);
		ids->cache = NULL;

		free(ids->diffopts_hash);
		ids->diffopts_hash = NULL;
	}

	return 0;
}

static int load_cached_patch_id(struct commit *commit,
		struct patch_ids *ids, unsigned char *sha1)
{
	const unsigned char *note_sha1;
	char *orig_note;
	char *note;
	enum object_type type;
	unsigned long notelen;
	int result = 1;

	if (!ids->cache)
		return 1;

	note_sha1 = get_note(ids->cache, commit->object.sha1);
	if (!note_sha1)
		return 1;

	if (!(orig_note = read_sha1_file(note_sha1, &type, &notelen)) ||
			!notelen || type != OBJ_BLOB)
		goto out;

	note = orig_note;
	if (get_sha1_hex(note, sha1))
		goto out;

	/* Advance past the patch ID */
	note += 41;
	/* Was the cached patch ID generated with the same diffopts? */
	if (strncmp(note, ids->diffopts_hash, ids->diffopts_hash_len)) {
		goto out;
	}

	note += ids->diffopts_hash_len;
	if (note[strlen(note) - 1] == '\n')
		note[strlen(note) - 1] = '\0';
	if (strcmp(note, git_version_string)) {
		struct notes_tree *new_cache;
		/*
		 * If the Git version has changed, throw away the entire
		 * caching notes tree on the assumption that the user will
		 * not return to the previous version.  We can bail out of
		 * this function sooner next time round if we don't find a
		 * note for the commit at all.
		 */
		new_cache = xcalloc(1, sizeof(*new_cache));
		init_notes(new_cache, ids->cache->ref,
			ids->cache->combine_notes, NOTES_INIT_EMPTY);
		free_notes(ids->cache);
		free(ids->cache);
		ids->cache = new_cache;
		goto out;
	}

	result = 0;
out:
	free(orig_note);
	return result;
}

static void save_cached_patch_id(struct commit *commit,
		struct patch_ids *ids, unsigned char *sha1)
{
	unsigned char note_sha1[20];
	struct strbuf sb = STRBUF_INIT;
	if (!ids->cache)
		return;

	strbuf_addstr(&sb, sha1_to_hex(sha1));
	strbuf_addch(&sb, '\n');
	strbuf_add(&sb, ids->diffopts_hash, ids->diffopts_hash_len);
	strbuf_addstr(&sb, git_version_string);
	strbuf_addch(&sb, '\n');

	if (write_sha1_file(sb.buf, sb.len, blob_type, note_sha1) ||
	    add_note(ids->cache, commit->object.sha1, note_sha1, NULL))
		error(_("unable to save patch ID in cache"));

	strbuf_release(&sb);
}

static struct patch_id *add_commit(struct commit *commit,
				   struct patch_ids *ids,
				   int no_add)
{
	struct patch_id_bucket *bucket;
	struct patch_id *ent;
	unsigned char sha1[20];
	int pos;

	if (load_cached_patch_id(commit, ids, sha1) &&
	    commit_patch_id(commit, &ids->diffopts, sha1))
		return NULL;
	pos = patch_pos(ids->table, ids->nr, sha1);
	if (0 <= pos)
		return ids->table[pos];
	save_cached_patch_id(commit, ids, sha1);
	if (no_add)
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

	if (ids->alloc <= ids->nr) {
		ids->alloc = alloc_nr(ids->nr);
		ids->table = xrealloc(ids->table, sizeof(ent) * ids->alloc);
	}
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
