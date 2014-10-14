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

#include <stdbool.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

#include <pthread.h>

#include <openssl/md5.h>

/* Needed for isvn-internal.h */
#include <svn_client.h>
#include <svn_pools.h>

#include "isvn/isvn-git2.h"
#include "isvn/isvn-internal.h"

#include "vcs-svn/line_buffer.h"
#include "vcs-svn/sliding_window.h"
#include "vcs-svn/svndiff.h"

#define BR_BUCKET(name) (strhash(name) % g_nr_commit_workers)

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
} *g_buckets;

static inline void
branch_lock(const char *name)
{
	mtx_lock(&g_buckets[BR_BUCKET(name)].bk_lock);
}

static inline void
branch_unlock(const char *name)
{
	mtx_unlock(&g_buckets[BR_BUCKET(name)].bk_lock);
}

static int
svn_branch_cmp(const struct svn_branch *b1, const struct svn_branch *b2,
    const void *dummy __unused)
{
	return strcmp(b1->br_name, b2->br_name);
}

struct svn_branch *
svn_branch_get(struct hashmap *h, const char *name)
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

static void
svn_branch_free(struct svn_branch *br)
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
static void
svn_branch_move_revs(struct svn_branch *dst, struct svn_branch *src)
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

void
svn_branch_revs_enqueue_and_free(struct svn_branch *branch)
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

void
svn_branch_hash_init(struct hashmap *hash)
{
	hashmap_init(hash, (hashmap_cmp_fn)svn_branch_cmp, 0);
}

void
svn_branch_append(struct svn_branch *sb, struct branch_rev *br)
{
	struct branch_rev *last;

	if (!TAILQ_EMPTY(&sb->br_revs)) {
		last = TAILQ_LAST(&sb->br_revs, revlist);
		if (last->rv_rev == br->rv_rev) {
			branch_rev_mergeinto(last, br);
			return;
		}
	}

	TAILQ_INSERT_TAIL(&sb->br_revs, br, rv_list);
}

static const unsigned char null_md5[16] = { 0 };
static bool
is_null_md5(unsigned char md5[16])
{
	return (memcmp(null_md5, md5, 16) == 0);
}

static void
isvn_md5(FILE *preimage, unsigned char md5[16])
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

static void
md5_die(unsigned rev, const char *prefix, const char *path,
    unsigned char exp[16], unsigned char actual[16])
{
	char mbufe[50], mbufa[50];

	md5_tostr(mbufe, exp);
	md5_tostr(mbufa, actual);

	//die("%s: %s got %s, expected %s!", prefix, path, mbufa, mbufe);
	printf("%s: %s@r%u got %s, expected %s!\n", prefix, path, rev, mbufa,
	    mbufe);
}

void
isvn_brancher_init(void)
{
	unsigned i;

	g_buckets = xcalloc(g_nr_commit_workers, sizeof(*g_buckets));
	for (i = 0; i < g_nr_commit_workers; i++) {
		mtx_init(&g_buckets[i].bk_lock);
		hashmap_init(&g_buckets[i].bk_branches,
			(hashmap_cmp_fn)svn_branch_cmp, 0);
	}
}

/* Wait until a workable branch exists in the bucket, and remove it.  If there
 * is nothing left that can be queued in this bucket, return NULL. */
static struct svn_branch *
get_workable_branch(struct svn_bucket *bk)
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

/* Look up a corresponding tree entry for a (potentially) multi-component path
 * (e.g. "a/b/c"). If not found in the root directory, 'aux' will hold the tree
 * that the returned entry points into. The caller is responsible for releasing
 * '*aux.'
 *
 * Returns NULL if there was no error but the entry doesn't exist. */
static const git_tree_entry *
isvn_lookup_tree_path_recursive(git_tree **aux, const git_tree *tree,
    const char *cpath)
{
	char *path, *s, spath[PATH_MAX];
	const git_tree_entry *ent;
	ssize_t path_rem, clen;
	int rc;

	path_rem = strlen(cpath);
	strlcpy(spath, cpath, sizeof(spath));
	path = spath;
	*aux = NULL;

	while (true) {
		s = strchrnul(path, '/');
		if (*s)
			*s = '\0';

		clen = s - path;
		/* INVARIANTS */
		if (clen == 0)
			die("bogus path: %s", cpath);

		ent = git_tree_entry_byname(tree, path);
		if (ent == NULL)
			break;
		else if (path_rem <= clen)
			break;

		path_rem -= clen + 1;
		path = s + 1;

		if (*aux)
			git_tree_free(*aux);

		rc = git_tree_lookup(aux, git_tree_owner(tree),
		    git_tree_entry_id(ent));
		if (rc < 0)
			die("git_tree_lookup: %d", rc);

		tree = *aux;
	}

	return ent;
}

static git_object *
isvn_lookup_copyfrom(git_repository *repo, const char *src_path,
    unsigned src_rev, git_oid *commit_sha1_out)
{
	const char *path, *src_branch;
	const git_tree_entry *ent;
	git_commit *comm;
	git_object *obj;
	git_tree *tree, *aux;
	git_oid sha1;
	int rc;

	path = strip_branch(src_path, &src_branch);
	sha1 = (git_oid) {};

	isvn_assert_commit(src_branch, src_rev);

	/* We want the latest rev on src_branch <= src_rev. */
	rc = isvn_revmap_lookup_branchlatest(src_branch, src_rev, &sha1);
	if (rc < 0) {
		if (option_verbosity >= 2)
			isvn_dump_revmap();
		die("%s: No such rev (r%u) for `%s'", __func__, src_rev,
		    src_branch);
	}

	if (commit_sha1_out)
		git_oid_cpy(commit_sha1_out, &sha1);

	rc = git_commit_lookup(&comm, repo, &sha1);
	if (rc < 0)
		die("git_commit_lookup");

	rc = git_commit_tree(&tree, comm);
	if (rc < 0)
		die("git_commit_lookup");

	git_commit_free(comm);

	/* Copying the whole tree (branch) at that rev? Easy! */
	if (strcmp(path, "") == 0) {
		rc = git_object_lookup(&obj, repo, git_tree_id(tree),
		    GIT_OBJ_TREE);
		if (rc < 0)
			die("git_object_lookup: %d", rc);

		git_tree_free(tree);
		return obj;
	}

	ent = isvn_lookup_tree_path_recursive(&aux, tree, path);
	if (ent == NULL)
		die("git_tree_entry_byname(%s) didn't find. "
		    "(Was: '%s' in %s@%u)", path, src_path, src_branch,
		    src_rev);

	git_oid_cpy(&sha1, git_tree_entry_id(ent));
	if (git_oid_iszero(&sha1))
		die("zero sha1 for valid obj?");

	rc = git_object_lookup(&obj, repo, &sha1, GIT_OBJ_ANY);
	if (rc < 0)
		die("git_object_lookup");
	if (git_object_type(obj) != GIT_OBJ_BLOB &&
	    git_object_type(obj) != GIT_OBJ_TREE)
		die("got unexpected index entry type: %s",
		    git_object_type2string(git_object_type(obj)));

	git_tree_free(aux);
	git_tree_free(tree);
	return obj;
}

static void
add_to_cache_internal(git_index *idx, struct branch_rev *rev, const char *path,
    const git_oid *sha1, unsigned mode, git_time_t rev_time)
{
	git_index_entry ce = {};
	git_otype otype;
	git_odb *odb;
	size_t len;
	int rc;

	ce.ctime = ce.mtime = (git_index_time) { .seconds = rev_time };

	if ((mode & 0777) == 0644)
		ce.mode = GIT_FILEMODE_BLOB;
	else if ((mode & 0777) == 0755)
		ce.mode = GIT_FILEMODE_BLOB_EXECUTABLE;
	else
		die("bogus mode %#o", mode);

	rc = git_repository_odb(&odb, git_index_owner(idx));
	if (rc < 0)
		die("git_repository_odb");
	rc = git_odb_read_header(&len, &otype, odb, sha1);
	if (rc < 0)
		die("git_odb_read_header");

	ce.file_size = len;
	git_oid_cpy(&ce.id, sha1);
	ce.path = path;

	ce.flags |= GIT_IDXENTRY_VALID;

	rc = git_index_add(idx, &ce);
	if (rc < 0)
		die("git_index_add: %d", rc);

	git_odb_free(odb);
}

static void
add_to_cache(git_index *idx, struct branch_rev *rev, struct br_edit *edit)
{
	const char *path;

	path = strip_branch(edit->e_path, NULL);
	add_to_cache_internal(idx, rev, path, &edit->e_new_sha1,
	    0644/*XXX*/, rev->rv_timestamp);
}

struct add_tree_ctx {
	git_index		*idx;
	struct branch_rev	*rev;
	const char		*atpath;
};

static int
add_tree_cb(const char *root, const git_tree_entry *entry, void *vctx)
{
	struct add_tree_ctx *ctx = vctx;
	char *sb;

	if (git_tree_entry_type(entry) == GIT_OBJ_TREE)
		return 0;
	else if (git_tree_entry_type(entry) != GIT_OBJ_BLOB)
		die("What file is this? %s%s = %#o (%d)", root,
		    git_tree_entry_name(entry), git_tree_entry_filemode(entry),
		    (int)git_tree_entry_type(entry));

	xasprintf(&sb, "%s%s%s%s", ctx->atpath, (*ctx->atpath)? "/" : "",
	    root, git_tree_entry_name(entry));

	add_to_cache_internal(ctx->idx, ctx->rev, sb, git_tree_entry_id(entry),
	    git_tree_entry_filemode(entry), ctx->rev->rv_timestamp);

	free(sb);
	return 0;
}

/* We want to skip the directory name of 'src_tree' itself, but otherwise add
 * its files, prefixed with 'atpath', to the index. */
static void
add_tree_to_cache(git_index *idx, struct branch_rev *rev, git_tree *src_tree,
    const char *atpath)
{
	struct add_tree_ctx ctx = {};
	int rc;

	ctx.idx = idx;
	ctx.rev = rev;
	ctx.atpath = atpath;

	rc = git_tree_walk(src_tree, GIT_TREEWALK_PRE /* we don't care */,
	    add_tree_cb, &ctx);
	if (rc < 0)
		die_errno("read_tree_recursive");
}

struct branch_context {
	const char		*name;
	const char		*remote;
	bool			 new_branch;
	unsigned		 svn_rev;

	char			*commit_log;
	git_oid			 sha1;
	git_repository		*git_repo;

	git_signature		*last_signature;
};

static int
isvn_readinto_blob(char *buf, size_t sz, void *v)
{
	int fd = (int)(intptr_t)v;
	ssize_t rd;

	rd = read(fd, buf, sz);
	if (rd < 0)
		return GIT_EUSER;
	return (int)rd;
}

static void
git_isvn_apply_edit(struct branch_context *ctx, struct branch_rev *rev,
    git_index *index, struct br_edit *edit)
{
	struct sliding_view preimage_view;
	struct line_buffer preimage, delta;
	char tmpbuf[PATH_MAX];
	bool preimage_opened;
	const char *path;
	FILE *fout;
	int rc, fd;

	git_blob *srcblob = NULL;
	git_object *from = NULL;
	git_tree *tfrom = NULL;
	git_oid commitsha1;

	fout = NULL;
	preimage_opened = false;
	delta = (struct line_buffer) LINE_BUFFER_INIT;
	preimage = (struct line_buffer) LINE_BUFFER_INIT;
	preimage_view = (struct sliding_view) SLIDING_VIEW_INIT(&preimage, -1);

	path = strip_branch(edit->e_path, NULL);

	if ((edit->e_kind & ED_DELETE) != 0) {
		rc = git_index_remove_directory(index, path, 0);
		if (rc < 0)
			die("git_index_remove_directory");
	}

	switch (edit->e_kind & ED_TYPEMASK) {
	case ED_MKDIR:
		if (edit->e_copyfrom == NULL)
			/* nothing to do. */
			break;

		from = isvn_lookup_copyfrom(ctx->git_repo, edit->e_copyfrom,
		    edit->e_copyrev, &commitsha1);
		rc = git_tree_lookup(&tfrom, ctx->git_repo, git_object_id(from));
		if (rc < 0)
			die("git_tree_lookup");

		/* If this copydir created a branch, let commit know. */
		if (strcmp(path, "") == 0) {
			if (!git_oid_iszero(&rev->rv_parent))
				die("copy over?");
			git_oid_cpy(&rev->rv_parent, &commitsha1);
		}

		add_tree_to_cache(index, rev, tfrom, path);
		break;

	case ED_NIL:
		/* nothing to do. */
		break;

	case ED_ADDFILE:
		if (edit->e_copyfrom) {
			from = isvn_lookup_copyfrom(ctx->git_repo,
			    edit->e_copyfrom, edit->e_copyrev, NULL);
			if (git_object_type(from) != GIT_OBJ_BLOB)
				die("lookup_copyfrom: %d != BLOB",
				    git_object_type(from));

			git_oid_cpy(&edit->e_new_sha1, git_object_id(from));
		}

		/* An ADD can (and usually? does) have a diff, but if not,
		 * we're finished here. */
		if (edit->e_diff == NULL) {
			if (edit->e_copyfrom == NULL) {
				/* TODO create if missing */
#if 0
				rc = git_blob_create_frombuffer(&edit->e_new_sha1, index, NULL, 0);
#endif
				die("Create empty blob");
			}

			/* Add the path to the index */
			add_to_cache(index, rev, edit);
			break;
		}

		if (edit->e_copyfrom) {
			rc = git_blob_lookup(&srcblob, ctx->git_repo,
			    &edit->e_new_sha1);
			if (rc < 0)
				die("git_blob_lookup");
			/* TODO file and git_odb_object_rstream */
			if (git_blob_rawsize(srcblob) > 0)
				buffer_meminit(&preimage,
				    /* fmemopen(3) takes non-const pointer for "w" */
				    __DECONST(git_blob_rawcontent(srcblob), void *),
				    git_blob_rawsize(srcblob));
			else
				/* Work around fmemopen(3) bug for NULL/0
				 * buffers: */
				buffer_fileinit(&preimage, fopen("/dev/null", "r"));

		} else
			/* preimage = empty. ED_TEXTDELTA will add file to index. */
			/* Work around fmemopen(3) bug for 0-length buffers: */
			buffer_fileinit(&preimage, fopen("/dev/null", "r"));

		preimage_opened = true;

		/* FALLTHROUGH */
	case ED_TEXTDELTA:
		if (!preimage_opened) {
			const git_index_entry *ce;

			ce = git_index_get_bypath(index, path, 0);
			if (ce == NULL)
				die("%s: Could not find blob (%s) in index to apply patch!",
				    __func__, path);

			git_oid_cpy(&edit->e_old_sha1, &ce->id);

			rc = git_blob_lookup(&srcblob, ctx->git_repo,
			    &edit->e_old_sha1);
			if (rc < 0)
				die("git_blob_lookup");

			/* TODO file and git_odb_object_rstream */
			if (git_blob_rawsize(srcblob) > 0)
				buffer_meminit(&preimage,
				    /* fmemopen(3) takes non-const pointer for "w" */
				    __DECONST(git_blob_rawcontent(srcblob), void *),
				    git_blob_rawsize(srcblob));
			else
				/* Work around fmemopen(3) bug for NULL/0
				 * buffers: */
				buffer_fileinit(&preimage, fopen("/dev/null", "r"));
			preimage_opened = true;
		}

		/* INVARIANTS */
		if (edit->e_diff == NULL)
			die("Textdelta without diff?? (%s, %s, %s)",
			    rev->rv_branch, __func__, edit->e_path);
		if (preimage_opened && preimage.infile == NULL)
			die_errno("Preimage(%d / %s) failed to open",
			    edit->e_kind, edit->e_path);

		rc = buffer_meminit(&delta, edit->e_diff, edit->e_difflen);
		if (rc)
			die("buffer_meminit");

		/* XXX respect $TMPDIR? */
		strcpy(tmpbuf, "/tmp/tmpXXXXXX");
		fd = mkstemp(tmpbuf);
		if (fd < 0)
			die("mkstemp: %s", strerror(errno));

		/* Preimage MD5 validation */
		if (!is_null_md5(edit->e_preimage_md5)) {
			unsigned char md5[16];

			isvn_md5(preimage.infile, md5);
			if (memcmp(md5, edit->e_preimage_md5, 16) != 0)
				md5_die(rev->rv_rev, "MD5 mismatch(preimage)",
				    edit->e_path, edit->e_preimage_md5, md5);
		}

		/* Apply the diff and create the new blob! */
		fout = fdopen(fd, "w+");
		rc = svndiff0_apply(&delta, edit->e_difflen, &preimage_view,
			fout);
		if (rc)
			die_errno("svndiff0_apply: %p %ld", preimage.infile,
			    ftell(preimage.infile));
		fflush(fout);
		rewind(fout);

		/* Postimage MD5 validation */
		if (!is_null_md5(edit->e_postimage_md5)) {
			unsigned char md5[16];

			isvn_md5(fout, md5);
			if (memcmp(md5, edit->e_postimage_md5, 16) != 0)
				md5_die(rev->rv_rev, "MD5 mismatch(postimage)",
				    edit->e_path, edit->e_postimage_md5, md5);
		}

		rc = git_blob_create_fromchunks(&edit->e_new_sha1,
		    ctx->git_repo, path, isvn_readinto_blob,
		    (void*)(intptr_t)fd);
		if (rc == GIT_EUSER)
			die("git_blob_create_fromchunks(%s): %s(%d)",
			    path, strerror(errno), errno);
		else if (rc < 0)
			die("git_blob_create_fromchunks: %d", rc);

		fclose(fout);
		rc = unlink(tmpbuf);
		if (rc < 0)
			die("couldn't remove temporary file %s: %s", tmpbuf,
			    strerror(errno));

		/* Add the path to the index */
		add_to_cache(index, rev, edit);
		break;

	case ED_PROP:
		die("Unhandled: prop-only change: %s, %s, %s\n",
		    rev->rv_branch, __func__, edit->e_path);

	default:
		die("%s: What? Bad edit kind: %#x\n", __func__, edit->e_kind);
	}

	git_blob_free(srcblob);
	git_object_free(from);
	git_tree_free(tfrom);

	buffer_deinit(&preimage);
	buffer_deinit(&delta);
	strbuf_release(&preimage_view.buf);
}

static int
git_isvn_apply_rev(struct branch_context *ctx, struct branch_rev *rev)
{
	struct br_edit *edit;
	int rc;

	git_tree *parent_tree, *new_tree;
	git_reference *parent_ref;
	git_object *parent_obj;
	git_oid new_tree_oid;
	char *rem_br, *at;
	git_index *index;

	git_commit *parents[2];
	size_t nparents;

	char tmpbuf[256];

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
		if (!isvn_has_commit(edit->e_copyrev))
			return -EBUSY;
	}

	/* TODO: Get libgit2 to create alternative / ephemoral indices. */
	rc = git_repository_index(&index, ctx->git_repo);
	if (rc < 0)
		die("git_repository_index");

	rc = git_index_clear(index);
	if (rc < 0)
		die("git_index_clear: %d (what?)", rc);

	xasprintf(&rem_br, "refs/remotes/%s", ctx->remote);

	rc = git_branch_lookup(&parent_ref, ctx->git_repo, ctx->remote,
	    GIT_BRANCH_REMOTE);
	if (rc < 0 && rc != GIT_ENOTFOUND)
		die("git_branch_lookup");

	if (rc == 0) {
		rc = git_reference_peel(&parent_obj, parent_ref, GIT_OBJ_TREE);
		if (rc < 0)
			die("git_reference_peel");

		rc = git_tree_lookup(&parent_tree, ctx->git_repo,
		    git_object_id(parent_obj));
		if (rc)
			die("git_tree_lookup");

		rc = git_index_read_tree(index, parent_tree);
		if (rc < 0)
			die("git_index_read_tree");
	} else {
		parent_ref = NULL;
		parent_obj = NULL;
		parent_tree = NULL;
	}

	/* Apply individual edits */
	TAILQ_FOREACH(edit, &rev->rv_editorder, e_list)
		git_isvn_apply_edit(ctx, rev, index, edit);

	rc = git_index_write_tree(&new_tree_oid, index);
	if (rc < 0)
		die("git_index_write_tree");

	rc = git_tree_lookup(&new_tree, ctx->git_repo, &new_tree_oid);
	if (rc < 0)
		die("git_tree_lookup");

	/* Lookup parent commit and start committing. */
	parents[0] = parents[1] = NULL;
	nparents = 0;
	if (!git_oid_iszero(&ctx->sha1)) {
		rc = git_commit_lookup(&parents[nparents++], ctx->git_repo, &ctx->sha1);
		if (rc < 0)
			die("git_lookup_commit");
	}
	if (!git_oid_iszero(&rev->rv_parent)) {
		rc = git_commit_lookup(&parents[nparents++], ctx->git_repo, &rev->rv_parent);
		if (rc < 0)
			die("git_lookup_commit");
	}

	if (ctx->commit_log) {
		free(ctx->commit_log);
		ctx->commit_log = NULL;
	}
	strlcpy(tmpbuf, rev->rv_logmsg, sizeof(tmpbuf));
	isvn_complete_line(tmpbuf, sizeof(tmpbuf));
	xasprintf(&ctx->commit_log, "%s\ngit-isvn-id: %s/%s@%u\n", tmpbuf,
	    g_repos_root, ctx->name, rev->rv_rev);

	/* reuse buf to strip <foo>@ from (maybe) email: */
	if (rev->rv_author) {
		at = strchrnul(rev->rv_author, '@');
		memcpy(tmpbuf, rev->rv_author, (at - rev->rv_author));
		tmpbuf[at - rev->rv_author] = '\0';

		if (ctx->last_signature) {
			git_signature_free(ctx->last_signature);
			ctx->last_signature = NULL;
		}
		rc = git_signature_new(&ctx->last_signature, tmpbuf,
		    rev->rv_author, rev->rv_timestamp, 0);
		if (rc < 0)
			die("git_signature_new");
	} else {
		rc = git_signature_new(&ctx->last_signature, "Nobody",
		    "null@nowhere", rev->rv_timestamp, 0);
		if (rc < 0)
			die("git_signature_new");
	}

	/* Create git commit object on tree, parent commit ! */
	rc = git_commit_create(&ctx->sha1, ctx->git_repo,
	    rem_br,
	    ctx->last_signature, ctx->last_signature, NULL, ctx->commit_log,
	    new_tree, nparents, (const git_commit **)parents);
	if (rc < 0)
		die("git_commit_create");

	char shabuf[50] = {};
	git_oid_fmt(shabuf, &ctx->sha1);
	printf("XXX %s: Wrote r%u as commit %s!\n", __func__, rev->rv_rev,
		shabuf);
	/* A real(-ish) revmap! */
	isvn_revmap_insert(rev->rv_rev, rev->rv_branch, &ctx->sha1);

	while (nparents)
		git_commit_free(parents[nparents--]);
	git_tree_free(new_tree);

	git_index_free(index);
	git_tree_free(parent_tree);
	git_object_free(parent_obj);
	git_reference_free(parent_ref);

	free(rem_br);
	return 0;
}

static int
git_isvn_apply_revs(struct svn_branch *sb, bool *committed_any)
{
	struct branch_context ctx = {};
	struct branch_rev *rev, *sr;
	git_reference *branchref;
	char *remote_branch;
	const git_oid *oldp;
	git_oid old_sha1;
	bool busy;
	int rc;

	busy = false;
	ctx.commit_log = NULL;
	xasprintf(&remote_branch, "%s/%s", option_origin, sb->br_name);

	ctx.git_repo = g_git_repo;

	old_sha1 = (git_oid) {};
	oldp = NULL;

	rc = git_branch_lookup(&branchref, ctx.git_repo, remote_branch,
	    GIT_BRANCH_REMOTE);
	if (rc < 0 && rc != GIT_ENOTFOUND)
		die("git_branch_lookup(%s): %d", remote_branch, rc);

	if (rc == 0)
		oldp = git_reference_target(branchref);

	if (oldp) {
		if (git_oid_iszero(oldp))
			die("null sha1 ref ???");
		git_oid_cpy(&old_sha1, oldp);
		git_oid_cpy(&ctx.sha1, oldp);

		/* XXX figure out svn_rev of the branch head. */
	} else
		ctx.new_branch = true;

	ctx.name = sb->br_name;
	ctx.remote = remote_branch;

	TAILQ_FOREACH_SAFE(rev, &sb->br_revs, rv_list, sr) {
		rc = git_isvn_apply_rev(&ctx, rev);
		if (rc < 0) {
			/* INVARIANTS */
			if (rc != -EBUSY)
				die("%s: rc=%d", __func__, rc);

			busy = true;
			break;
		}

		/* XXX Could batch them up, but be wary that ascending revs on
		 * a branch aren't neccessarily a contiguous range. */
		isvn_mark_commitdone(rev->rv_rev, rev->rv_rev);
		*committed_any = true;

		TAILQ_REMOVE(&sb->br_revs, rev, rv_list);
		branch_rev_free(rev);
	}

	git_signature_free(ctx.last_signature);
	git_reference_free(branchref);
	free(remote_branch);
	free(ctx.commit_log);
	return (busy? -EBUSY : 0);
}

void *isvn_bucket_worker(void *v)
{
	unsigned bk_i = (unsigned)(uintptr_t)v;
	struct svn_bucket *bk = &g_buckets[bk_i];
	unsigned busies;
	bool deadlock;
	int rc;

	deadlock = false;
	busies = 0;

	while (true) {
		struct svn_branch *br;
		bool committed_any;

		br = get_workable_branch(bk);
		if (br == NULL)
			break;

		committed_any = false;
		rc = git_isvn_apply_revs(br, &committed_any);
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

		if (committed_any) {
			busies = 0;
		} else {
			busies++;

			mtx_lock(&bk->bk_lock);
			if (busies >= 2 * bk->bk_branches.hm_size)
				deadlock = true;
			mtx_unlock(&bk->bk_lock);
		}

		if (deadlock) {
			printf("Branch-worker deadlock detected:\n");
			isvn_commitdone_dump();

			/* XXX Print each's dependencies, current commitdone,
			 * ... */
			die("Aborting.");
		}
	}

	return NULL;
}
