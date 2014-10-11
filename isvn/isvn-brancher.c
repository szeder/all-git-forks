/*
 * Implementation of isvn parallel patch-applying.
 *
 * Copyright (c) 2014 Conrad Meyer <cse.cem@gmail.com>
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <stdbool.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

#include <openssl/md5.h>

/* Needed for isvn-internal.h */
#include <svn_client.h>
#include <svn_pools.h>

#define NO_THE_INDEX_COMPATIBILITY_MACROS

#include "builtin.h"
#include "cache.h"
#include "cache-tree.h"
#include "dir.h"
#include "parse-options.h"
#include "pathspec.h"
#include "refs.h"
#include "remote.h"
#include "streaming.h"
#include "thread-utils.h"
#include "transport.h"
#include "tree-walk.h"
#include "help.h"

#include "vcs-svn/line_buffer.h"
#include "vcs-svn/sliding_window.h"
#include "vcs-svn/svndiff.h"

#include "isvn/isvn-internal.h"

#define BR_BUCKET(name) (strhash(name) % NR_WORKERS)

struct svn_branch {
	struct hashmap_entry		 br_entry;
	char				*br_name;
	TAILQ_HEAD(revlist, branch_rev)	 br_revs;
};

/* Each bucket-worker owns a bucket. */
struct svn_bucket {
	pthread_mutex_t bk_lock;
	/* Consider LRU or something to limit # */
	struct hashmap bk_branches;	/* (b) */
} g_buckets[NR_WORKERS];

static inline void branch_lock(const char *name)
{
	mtx_lock(&g_buckets[BR_BUCKET(name)].bk_lock);
}

static inline void branch_unlock(const char *name)
{
	mtx_unlock(&g_buckets[BR_BUCKET(name)].bk_lock);
}

static int svn_branch_cmp(const struct svn_branch *b1,
	const struct svn_branch *b2, const void *dummy __unused)
{
	return strcmp(b1->br_name, b2->br_name);
}

struct svn_branch *svn_branch_get(struct hashmap *h, const char *name)
{
	struct svn_branch *b, blookup;

	blookup.br_name = __DECONST(name, char *);
	hashmap_entry_init(&blookup.br_entry, strhash(name));

	b = hashmap_get(h, &blookup, NULL);
	if (b)
		return b;

	b = xmalloc(sizeof(*b));
	if (b == NULL)
		die("malloc");

	memset(b, 0, sizeof(*b));
	b->br_name = xstrdup(name);
	hashmap_entry_init(&b->br_entry, strhash(name));
	TAILQ_INIT(&b->br_revs);

	hashmap_add(h, b);
	return b;
}

static void svn_branch_free(struct svn_branch *br)
{
	struct branch_rev *rev, *tmp;

	TAILQ_FOREACH_SAFE(rev, &br->br_revs, rv_list, tmp) {
		TAILQ_REMOVE(&br->br_revs, rev, rv_list);
		branch_rev_free(rev);
	}

	/* INVARIANTS */
	if (!TAILQ_EMPTY(&br->br_revs))
		die("%s: non-empty branch %s", __func__, br->br_name);

	free(br->br_name);
	memset(br, 0xfd, sizeof(*br));
	free(br);
}

/* Move revs from src to dst, keeping dst sorted. */
static void svn_branch_move_revs(struct svn_branch *dst, struct svn_branch *src)
{
	TAILQ_HEAD(, branch_rev) tmphd;
	struct branch_rev *rev, *it;
	size_t i;

	if (TAILQ_EMPTY(&src->br_revs))
		die("%s: What are you doing", __func__);

	if (TAILQ_EMPTY(&dst->br_revs)) {
		TAILQ_CONCAT(&dst->br_revs, &src->br_revs, rv_list);
		return;
	}

	/* If the src list proceeds all of dst */
	if (TAILQ_FIRST(&dst->br_revs)->rv_rev >
		TAILQ_FIRST(&src->br_revs)->rv_rev) {
		TAILQ_CONCAT(&src->br_revs, &dst->br_revs, rv_list);
		TAILQ_SWAP(&src->br_revs, &dst->br_revs, branch_rev, rv_list);
		return;
	}

	/* Find the right place in dst to insert src after */
	i = 0;
	TAILQ_FOREACH(rev, &dst->br_revs, rv_list) {
		if (rev->rv_rev > TAILQ_FIRST(&src->br_revs)->rv_rev)
			break;
		it = rev;
		i++;
	}

	/* List surgery: Splice remainder to tmphd, concat src, then concat
	 * remainder back on. */
	TAILQ_INIT(&tmphd);
	TAILQ_SPLICE(&tmphd, &dst->br_revs, it, rv_list);
	TAILQ_CONCAT(&dst->br_revs, &src->br_revs, rv_list);
	TAILQ_CONCAT(&dst->br_revs, &tmphd, rv_list);
}

void svn_branch_revs_enqueue_and_free(struct svn_branch *branch)
{
	struct svn_bucket *bk;
	struct svn_branch *gbranch;

	bk = &g_buckets[BR_BUCKET(branch->br_name)];

	/* Accumulate local branch_revs to global */
	branch_lock(branch->br_name);
	gbranch = svn_branch_get(&bk->bk_branches, branch->br_name);
	svn_branch_move_revs(gbranch, branch);
	branch_unlock(branch->br_name);

	svn_branch_free(branch);
}

void svn_branch_hash_init(struct hashmap *hash)
{
	hashmap_init(hash, (hashmap_cmp_fn)svn_branch_cmp, 0);
}

void svn_branch_append(struct svn_branch *sb, struct branch_rev *br)
{
	TAILQ_INSERT_TAIL(&sb->br_revs, br, rv_list);
}

static const unsigned char null_md5[16] = { 0 };
static bool is_null_md5(unsigned char md5[16])
{
	return (memcmp(null_md5, md5, 16) == 0);
}

static void isvn_md5(FILE *preimage, unsigned char md5[16])
{
	char buf[16* 1024];
	MD5_CTX c;

	rewind(preimage);

	MD5_Init(&c);
	for (;;) {
		size_t rd = fread(buf, 1, sizeof(buf), preimage);

		MD5_Update(&c, buf, rd);
		if (rd == 0)
			break;
	}
	MD5_Final(md5, &c);
	rewind(preimage);
}

static void md5_die(const char *prefix, const char *path,
	unsigned char exp[16], unsigned char actual[16])
{
	char mbufe[50], mbufa[50];

	bin_to_hex_buf(exp, mbufe, 16);
	bin_to_hex_buf(actual, mbufa, 16);

	//die("%s: %s got %s, expected %s!", prefix, path, mbufa, mbufe);
	printf("%s: %s got %s, expected %s!\n", prefix, path, mbufa, mbufe);
}

void isvn_brancher_init(void)
{
	unsigned i;

	for (i = 0; i < NR_WORKERS; i++) {
		mtx_init(&g_buckets[i].bk_lock);
		hashmap_init(&g_buckets[i].bk_branches,
			(hashmap_cmp_fn)svn_branch_cmp, 0);
	}
}

/* Wait until a workable branch exists in the bucket, and remove it.  If there
 * is nothing left that can be queued in this bucket, return NULL. */
static struct svn_branch *get_workable_branch(struct svn_bucket *bk)
{
	struct svn_branch *branch, *bb = NULL;
	struct hashmap_iter iter;
	unsigned lowest_rev = UINT_MAX, brev;
	bool done;

	done = false;
	mtx_lock(&bk->bk_lock);
	while (true) {
		hashmap_iter_init(&bk->bk_branches, &iter);
		while ((branch = hashmap_iter_next(&iter))) {
			if (TAILQ_EMPTY(&branch->br_revs))
				continue;

			brev = TAILQ_FIRST(&branch->br_revs)->rv_rev;
			if (brev >= lowest_rev)
				continue;

			/* Just a heuristic; we may end up waiting for earlier
			 * revisions: */
			lowest_rev = brev;
			bb = branch;
		}

		/* It's okay to not 'claim' this branch by deleting it under
		 * lock becaues we aren't contesting any other worker threads
		 * -- each one owns a bucket. The only other accessor on
		 * bk_lock is the fetcher threads, and they will only ever
		 * insert more revs. */
		if (bb)
			break;

		if (done)
			break;

		/* This is not the cleanest threaded API. I apologize. */
		mtx_unlock(&bk->bk_lock);

		/* If the fetchers have finished, do another pass through the
		 * branches on this bucket */
		if (isvn_all_fetched())
			done = true;
		else
			usleep(50);

		mtx_lock(&bk->bk_lock);
	}
	mtx_unlock(&bk->bk_lock);

	if (bb) {
		unsigned lastrev = 0, rev;

		mtx_lock(&bk->bk_lock);
		while (true) {
			/* Keep waiting for later revs if more stuff is
			 * appended. */
			rev = TAILQ_LAST(&bb->br_revs, revlist)->rv_rev;
			if (rev == lastrev)
				break;
			lastrev = rev;
			mtx_unlock(&bk->bk_lock);

			isvn_wait_fetch(lastrev);

			mtx_lock(&bk->bk_lock);
		}
		hashmap_remove(&bk->bk_branches, bb, NULL);
		mtx_unlock(&bk->bk_lock);
	}

	return bb;
}

struct copyfrom_ctx {
	unsigned char	(*sha1)[20];
	const char	 *path;
	unsigned	  hits;
};

static int copyfrom_lookup_cb(const unsigned char *sha1, const char *base,
	int baselen, const char *path, unsigned mode __unused,
	int stage __unused, void *vctx)
{
	struct copyfrom_ctx *ctx = vctx;

	if (strncmp(ctx->path, base, baselen) != 0) {
		printf("XXX %s: What are you doing pathspec1? %.*s / %s\n",
			__func__, baselen, base, ctx->path);
		return -1;
	}
	if (strncmp(ctx->path + baselen, path, strlen(path)) == 0 &&
		strcmp(ctx->path + baselen, path) != 0) {
		return READ_TREE_RECURSIVE;
	}

	ctx->hits++;
	hashcpy(*ctx->sha1, sha1);
	return 0;
}

static struct object *isvn_lookup_copyfrom(const char *src_path,
	unsigned src_rev, unsigned char *commit_sha1_out)
{
	const char *path, *src_branch;
	struct copyfrom_ctx ctx = {};
	struct pathspec pathspec;
	unsigned char sha1[20];
	struct tree *tree;
	int rc;

	path = strip_branch(src_path, &src_branch);
	hashclr(sha1);

	isvn_assert_commit(src_branch, src_rev);

	/* We want the latest rev on src_branch <= src_rev. */
	rc = isvn_revmap_lookup_branchlatest(src_branch, src_rev, sha1);
	if (rc < 0) {
		if (option_verbosity >= 2)
			isvn_dump_revmap();
		die("%s: No such rev (r%u) for `%s'", __func__,
			src_rev, src_branch);
	}

	if (commit_sha1_out)
		hashcpy(commit_sha1_out, sha1);

	tree = parse_tree_indirect(sha1);

	/* Copying the whole tree (branch) at that rev? Easy! */
	if (strcmp(path, "") == 0)
		return &tree->object;

	/* Unclear what PATHSPEC_PREFER_CWD assert in parse_pathspec refers
	 * to. */
	parse_pathspec(&pathspec, 0,
		PATHSPEC_LITERAL_PATH | PATHSPEC_PREFER_CWD, path, NULL);

	ctx.sha1 = &sha1;
	ctx.path = path;
	ctx.hits = 0;
	hashclr(sha1);

	rc = read_tree_recursive(tree, NULL, 0, 0, &pathspec,
		copyfrom_lookup_cb, &ctx);
	if (rc < 0)
		die_errno("read_tree_recursive");

	/* INVARIANTS */
	if (ctx.hits != 1)
		die("Aborting, multiple paths (%u) matched %s?!", ctx.hits,
			path);

	free_pathspec(&pathspec);
	if (is_null_sha1(sha1))
		die("Didn't find '%s' (was: '%s') in %s@%u?", path, src_path,
			src_branch, src_rev);

	return parse_object(sha1);
}

static void add_to_cache_internal(struct index_state *idx, struct branch_rev *rev,
	const char *path, const unsigned char sha1[20], unsigned mode)
{
	struct cache_entry *ce;
	size_t len, sz;

	len = strlen(path);
	sz = cache_entry_size(len);

	ce = xcalloc(1, sz);

	hashcpy(ce->sha1, sha1);
	memcpy(ce->name, path, len);
	ce->ce_flags = create_ce_flags(0);
	ce->ce_namelen = len;
	ce->ce_mode = mode;
	ce->ce_flags |= CE_VALID;

	if (add_index_entry(idx, ce,
		ADD_CACHE_OK_TO_ADD | ADD_CACHE_OK_TO_REPLACE))
		die("add_index_entry");
}

static void add_to_cache(struct index_state *idx, struct branch_rev *rev,
	struct br_edit *edit)
{
	const char *path;

	path = strip_branch(edit->e_path, NULL);
	add_to_cache_internal(idx, rev, path, edit->e_new_sha1,
		create_ce_mode(0644));
}

struct add_tree_ctx {
	struct index_state	*idx;
	struct branch_rev	*rev;
	const char		*atpath;
};

static int add_tree_cb(const unsigned char *sha1, const char *base,
	int baselen, const char *path, unsigned mode, int stage __unused,
	void *vctx)
{
	struct add_tree_ctx *ctx = vctx;
	struct strbuf sb;

	if (S_ISDIR(mode))
		return READ_TREE_RECURSIVE;
	if (!S_ISREG(mode))
		die("What mode is this? %.*s%s = %#o", baselen, base, path,
			mode);

	sb = (struct strbuf) STRBUF_INIT;
	strbuf_addf(&sb, "%s", ctx->atpath);
	if (sb.len > 0)
		strbuf_addch(&sb, '/');
	strbuf_addf(&sb, "%.*s%s", baselen, base, path);

	add_to_cache_internal(ctx->idx, ctx->rev, sb.buf, sha1, mode);

	strbuf_release(&sb);
	return 0;
}

/* We want to skip the directory name of 'src_tree' itself, but otherwise add
 * its files, prefixed with 'atpath', to the index. */
static void add_tree_to_cache(struct index_state *idx, struct branch_rev *rev,
	struct tree *src_tree, const char *atpath)
{
	struct add_tree_ctx ctx = {};
	struct pathspec pathspec;
	int rc;

	ctx.idx = idx;
	ctx.rev = rev;
	ctx.atpath = atpath;

	parse_pathspec(&pathspec, 0,
		PATHSPEC_PREFER_CWD | PATHSPEC_MAXDEPTH_VALID, "", NULL);
	pathspec.max_depth = -1;

	rc = read_tree_recursive(src_tree, NULL, 0, 0, &pathspec, add_tree_cb,
		&ctx);
	if (rc < 0)
		die_errno("read_tree_recursive");

	free_pathspec(&pathspec);
}

struct branch_context {
	const char		*name;
	const char		*remote;
	unsigned char		 sha1[20];
	bool			 new_branch;
	unsigned		 svn_rev;
	struct strbuf		 commit_log;
};

static void git_isvn_apply_edit(struct branch_context *ctx,
	struct branch_rev *rev, struct index_state *idx, struct br_edit *edit)
{
	struct sliding_view preimage_view;
	struct line_buffer preimage, delta;
	unsigned char commitsha1[20];
	char tmpbuf[PATH_MAX];
	bool preimage_opened;
	struct object *from;
	struct tree *tfrom;
	const char *path;
	struct stat sb;
	FILE *fout, *f;
	int rc, fd;
	unsigned i;

	fout = NULL;
	preimage_opened = false;
	delta = (struct line_buffer) LINE_BUFFER_INIT;
	preimage = (struct line_buffer) LINE_BUFFER_INIT;
	preimage_view = (struct sliding_view) SLIDING_VIEW_INIT(&preimage, -1);

	path = strip_branch(edit->e_path, NULL);

	if ((edit->e_kind & ED_DELETE) != 0) {
		for (i = 0; i < idx->cache_nr; i++) {
			if (path_startswith(idx->cache[i]->name, path)) {
				cache_tree_invalidate_path(idx,
					idx->cache[i]->name);
				idx->cache[i]->ce_flags |= CE_REMOVE;
			}
		}
		remove_marked_cache_entries(idx);
	}

	switch (edit->e_kind & ED_TYPEMASK) {
	case ED_MKDIR:
		for (i = 0; i < idx->cache_nr; i++)
			if (strcmp(idx->cache[i]->name, path) == 0)
				idx->cache[i]->ce_flags &= ~CE_REMOVE;

		if (edit->e_copyfrom == NULL)
			/* nothing to do. */
			break;

		from = isvn_lookup_copyfrom(edit->e_copyfrom, edit->e_copyrev,
			commitsha1);
		tfrom = object_as_type(from, OBJ_TREE, false);
		if (tfrom == NULL)
			die("lookup_copyfrom");

		/* If this copydir created a branch, let git know. */
		if (strcmp(path, "") == 0) {
			if (!is_null_sha1(rev->rv_parent))
				die("copy over?");
			hashcpy(rev->rv_parent, commitsha1);
		}

		add_tree_to_cache(idx, rev, tfrom, path);
		break;

	case ED_NIL:
		/* nothing to do. */
		break;

	case ED_ADDFILE:
		for (i = 0; i < idx->cache_nr; i++)
			if (strcmp(idx->cache[i]->name, path) == 0)
				idx->cache[i]->ce_flags &= ~CE_REMOVE;

		if (edit->e_copyfrom) {
			struct object *from;

			from = isvn_lookup_copyfrom(edit->e_copyfrom,
				edit->e_copyrev, NULL);
			if (from->type != OBJ_BLOB)
				die("lookup_copyfrom: %d != BLOB", from->type);

			hashcpy(edit->e_new_sha1, from->sha1);
		}

		/* An ADD can (and usually? does) have a diff, but if not,
		 * we're finished here. */
		if (edit->e_diff == NULL) {
			if (edit->e_copyfrom == NULL) {
				/* TODO create if missing */
				hashcpy(edit->e_new_sha1, EMPTY_BLOB_SHA1_BIN);
				die("Create empty blob");
			}

			/* Add the path to the index */
			add_to_cache(idx, rev, edit);
			break;
		}

		if (edit->e_copyfrom) {
			f = fopen_blob_stream(edit->e_new_sha1, NULL);
			if (f == NULL)
				die_errno("fopen_blob_stream2");
			buffer_fileinit(&preimage, f);
		} else
			/* preimage = empty. ED_TEXTDELTA will add file to index. */
			buffer_meminit(&preimage, NULL, 0);

		preimage_opened = true;

		/* FALLTHROUGH */
	case ED_TEXTDELTA:
		if (!preimage_opened) {
			struct cache_entry *ce;

			ce = index_file_exists(idx, path, strlen(path), false);
			if (ce == NULL)
				die("%s: Could not find blob (%s) in index to apply patch!",
					__func__, path);

			hashcpy(edit->e_old_sha1, ce->sha1);

			f = fopen_blob_stream(edit->e_old_sha1, NULL);
			if (f == NULL)
				die_errno("fopen_blob_stream");

			buffer_fileinit(&preimage, f);
			preimage_opened = true;
		}

		/* INVARIANTS */
		if (edit->e_diff == NULL)
			die("Textdelta without diff?? (%s, %s, %s)",
			    rev->rv_branch, __func__, edit->e_path);

		rc = buffer_meminit(&delta, edit->e_diff, edit->e_difflen);
		if (rc)
			die("buffer_meminit");

		fd = git_mkstemp(tmpbuf, sizeof(tmpbuf), "tmpXXXXXXXXXX");
		if (fd < 0)
			die("git_mkstemp: %s", strerror(errno));

		/* Preimage MD5 validation */
		if (!is_null_md5(edit->e_preimage_md5)) {
			unsigned char md5[16];

			isvn_md5(preimage.infile, md5);
			if (memcmp(md5, edit->e_preimage_md5, 16) != 0)
				md5_die("MD5 mismatch(preimage)", edit->e_path,
					edit->e_preimage_md5, md5);
		}

		/* Apply the diff and create the new blob! */
		fout = fdopen(fd, "w+");
		rc = svndiff0_apply(&delta, edit->e_difflen, &preimage_view,
			fout);
		if (rc)
			die("svndiff0_apply");
		fflush(fout);
		rewind(fout);

		/* Postimage MD5 validation */
		if (!is_null_md5(edit->e_postimage_md5)) {
			unsigned char md5[16];

			isvn_md5(fout, md5);
			if (memcmp(md5, edit->e_postimage_md5, 16) != 0)
				md5_die("MD5 mismatch(postimage)", edit->e_path,
					edit->e_postimage_md5, md5);
		}

		rc = fstat(fd, &sb);
		if (rc < 0)
			die("fstat");

		rc = index_fd(edit->e_new_sha1, fd, &sb, OBJ_BLOB, NULL,
			HASH_FORMAT_CHECK | HASH_WRITE_OBJECT);
		if (rc < 0)
			die("index_fd");

		fclose(fout);
		rc = unlink(tmpbuf);
		if (rc < 0)
			die("couldn't remove temporary file %s", tmpbuf);

		/* Add the path to the index */
		add_to_cache(idx, rev, edit);
		break;

	case ED_PROP:
		die("Unhandled: prop-only change: %s, %s, %s\n",
		    rev->rv_branch, __func__, edit->e_path);

	default:
		die("%s: What? Bad edit kind: %#x\n", __func__, edit->e_kind);
	}

	buffer_deinit(&preimage);
	buffer_deinit(&delta);
	strbuf_release(&preimage_view.buf);
}

static int git_isvn_apply_rev(struct branch_context *ctx,
	struct branch_rev *rev)
{
	struct strbuf authorish = STRBUF_INIT;
	char *idx_file, safebranch[PATH_MAX];
	struct index_state branch_idx = {};
	struct commit_list *parents;

	struct lock_file *lock;
	struct br_edit *edit;
	int lkfd, rc;
	unsigned i;

	if (ctx->svn_rev >= rev->rv_rev) {
		if (option_verbosity >= 0)
			printf("W: Skipping already-applied revision r%u\n",
			    rev->rv_rev);
		return 0;
	}

	if (rev->rv_only_empty_dirs) {
		printf("XXX %s: Empty dirs only! Nothing to do. (r%u)\n",
			__func__, rev->rv_rev);
		return 0;
	}

	/* Scan for needed copy sources; if not found, can't progress on this
	 * branch yet. */
	TAILQ_FOREACH(edit, &rev->rv_editorder, e_list) {
		const char *src_branch;

		if (edit->e_copyfrom == NULL)
			continue;

		(void) strip_branch(edit->e_copyfrom, &src_branch);
		if (!isvn_has_commit(src_branch, edit->e_copyrev))
			return -EBUSY;
	}

	/* Create a safeish index-file name for this branch ... */
	strlcpy(safebranch, rev->rv_branch, sizeof(safebranch));
	for (i = 0; safebranch[i] != '\0'; i++) {
		if (safebranch[i] == '/')
			safebranch[i] = '-';
	}
	/* TODO: Re-creating the index from the last commit tree should be
	 * super cheap. Do this instead of persisting the index for every
	 * branch. (See: checkout() in cmd_isvn_clone(). */
	idx_file = git_pathdup(".tmp_index--%s", safebranch);

	/* Create index */
	lock = xcalloc(1, sizeof(*lock));
	lkfd = hold_lock_file_for_update(lock, idx_file, 0);
	if (lkfd < 0)
		die("%s: lock %s: %s\n", __func__, idx_file, strerror(errno));

	rc = read_index_from(&branch_idx, idx_file);
	if (rc < 0)
		die("read_index_from");

	/* Apply individual edits */
	TAILQ_FOREACH(edit, &rev->rv_editorder, e_list)
		git_isvn_apply_edit(ctx, rev, &branch_idx, edit);

	/* Write index to tree object */
	if (branch_idx.cache_tree == NULL)
		branch_idx.cache_tree = cache_tree();

	rc = cache_tree_update(&branch_idx, 0);
	if (rc < 0)
		die("XXX unmerged index");

	rc = write_locked_index(&branch_idx, lock, COMMIT_LOCK);
	if (rc < 0)
		die("write_locked_index: %s", idx_file);

	rollback_lock_file(lock);

	/* Lookup parent commit and start committing. */
	parents = NULL;
	if (!is_null_sha1(ctx->sha1))
		commit_list_insert(lookup_commit(ctx->sha1), &parents);
	if (!is_null_sha1(rev->rv_parent))
		commit_list_insert(lookup_commit(rev->rv_parent), &parents);

	strbuf_reset(&ctx->commit_log);
	strbuf_addf(&ctx->commit_log, "%s", rev->rv_logmsg);
	strbuf_complete_line(&ctx->commit_log);
	strbuf_addch(&ctx->commit_log, '\n');
	strbuf_addf(&ctx->commit_log, "git-isvn-id: %s/%s@%u\n", g_repos_root,
		ctx->name, rev->rv_rev);

	/* XXX better svn user -> git email-style translation */
	/* Incredibly, date is part of the authorish string! */
	strbuf_addf(&authorish, "Unknown Person <%s> %lu +0000", rev->rv_author,
		rev->rv_timestamp);

	/* Create git commit object on tree, parent commit ! */
	isvn_g_lock();		/* commit is not thread-safe ... */
	rc = commit_tree(ctx->commit_log.buf, ctx->commit_log.len,
		branch_idx.cache_tree->sha1, parents, ctx->sha1,
		authorish.buf, NULL);
	isvn_g_unlock();
	if (rc < 0)
		die("commit_tree");

	strbuf_release(&authorish);

	char tmpbuf[50];
	bin_to_hex_buf(ctx->sha1, tmpbuf, 20);
	printf("XXX %s: Wrote r%u as commit %s!\n", __func__, rev->rv_rev,
		tmpbuf);
	/* A real(-ish) revmap! */
	if (!rev->rv_secondary)
		isvn_revmap_insert(rev->rv_rev, rev->rv_branch, ctx->sha1);

	discard_index(&branch_idx);
	/* TODO: Reconstruct index from tree... */
#if 0
	unlink(idx_file);
#endif
	free(idx_file);
	return 0;
}

static int git_isvn_apply_revs(struct svn_branch *sb)
{
	struct strbuf remote_branch = STRBUF_INIT,
		      reflog_msg = STRBUF_INIT;
	struct branch_rev *rev, *sr;
	const char *nl;
	int rc, flags;
	bool busy;

	struct branch_context ctx = {};
	unsigned char old_sha1[20];

	busy = false;
	ctx.commit_log = (struct strbuf) STRBUF_INIT;
	strbuf_addf(&remote_branch, "refs/remotes/%s/%s", option_origin,
		sb->br_name);

	flags = 0;
	/* XXX Lock around shitty thread-unsafe ref code */
	isvn_g_lock();
	rc = read_ref_full(remote_branch.buf, old_sha1, 1, &flags);
	isvn_g_unlock();
	if (flags & REF_ISSYMREF)
		die("what? branch %s is symbolic?", remote_branch.buf);
	if (flags & REF_ISBROKEN)
		die("branch %s is broken", remote_branch.buf);
	if (rc == 0) {
		if (is_null_sha1(old_sha1))
			die("null sha1 ref ???");
		hashcpy(ctx.sha1, old_sha1);

		/* XXX figure out svn_rev of the branch head. */
	} else
		ctx.new_branch = true;

	ctx.name = sb->br_name;
	ctx.remote = remote_branch.buf;

	TAILQ_FOREACH_SAFE(rev, &sb->br_revs, rv_list, sr) {
		rc = git_isvn_apply_rev(&ctx, rev);
		if (rc < 0) {
			/* INVARIANTS */
			if (rc != -EBUSY)
				die("%s: rc=%d", __func__, rc);

			busy = true;
			break;
		}

		TAILQ_REMOVE(&sb->br_revs, rev, rv_list);
		branch_rev_free(rev);
	}

	/* Nothing was committed? Nothing to ref. */
	if (ctx.commit_log.len == 0)
		goto out;

	/* Update branch ref to this sha1 */
	if (ctx.new_branch)
		strbuf_addf(&reflog_msg, "commit (initial)");
	else
		strbuf_addf(&reflog_msg, "commit");

	/* Format reflog txn message:
	 * "commit: <first line of last commit>\n" */
	nl = strchrnul(ctx.commit_log.buf, '\n');
	strbuf_addf(&reflog_msg, ": ");
	strbuf_add(&reflog_msg, ctx.commit_log.buf, nl - ctx.commit_log.buf);
	strbuf_addch(&reflog_msg, '\n');

	/* XXX Lock around shitty thread-unsafe ref code */
	isvn_g_lock();
	rc = update_ref(reflog_msg.buf, remote_branch.buf, ctx.sha1,
		ctx.new_branch? NULL : old_sha1, 0, UPDATE_REFS_DIE_ON_ERR);
	isvn_g_unlock();
	if (rc)
		die("update_ref");

out:
	strbuf_release(&remote_branch);
	strbuf_release(&ctx.commit_log);
	strbuf_release(&reflog_msg);
	return (busy? -EBUSY : 0);
}

void *isvn_bucket_worker(void *v)
{
	unsigned bk_i = (unsigned)(uintptr_t)v;
	struct svn_bucket *bk = &g_buckets[bk_i];
	int rc;

	while (true) {
		struct svn_branch *br;

		br = get_workable_branch(bk);
		if (br == NULL)
			break;

		rc = git_isvn_apply_revs(br);
		if (rc < 0) {
			/* INVARIANTS */
			if (rc != -EBUSY)
				die("%s: rc=%d", __func__, rc);
			if (TAILQ_EMPTY(&br->br_revs))
				die("%s: empty but busy?", __func__);

			/* Put dependent revs back on the branch queue. */
			svn_branch_revs_enqueue_and_free(br);
		} else
			svn_branch_free(br);
	}

	return NULL;
}
