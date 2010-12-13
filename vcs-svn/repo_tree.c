/*
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "git-compat-util.h"
#include "strbuf.h"
#include "repo_tree.h"
#include "fast_export.h"

const char *repo_read_path(const char *path, uint32_t *mode_out)
{
	static struct strbuf buf = STRBUF_INIT;

<<<<<<< HEAD
static struct repo_dir *repo_commit_root_dir(struct repo_commit *commit)
{
	return dir_pointer(commit->root_dir_offset);
}

static struct repo_dirent *repo_first_dirent(struct repo_dir *dir)
{
	return dent_first(&dir->entries);
}

static int repo_dirent_name_cmp(const void *a, const void *b)
{
	const struct repo_dirent *dent1 = a, *dent2 = b;
	uint32_t a_offset = dent1->name_offset;
	uint32_t b_offset = dent2->name_offset;
	return (a_offset > b_offset) - (a_offset < b_offset);
}

static int repo_dirent_is_dir(struct repo_dirent *dent)
{
	return dent != NULL && dent->mode == REPO_MODE_DIR;
}

static struct repo_dir *repo_dir_from_dirent(struct repo_dirent *dent)
{
	if (!repo_dirent_is_dir(dent))
		return NULL;
	return dir_pointer(dent->content_offset);
}

static struct repo_dir *repo_clone_dir(struct repo_dir *orig_dir)
{
	uint32_t orig_o, new_o;
	orig_o = dir_offset(orig_dir);
	if (orig_o >= dir_pool.committed)
		return orig_dir;
	new_o = dir_alloc(1);
	orig_dir = dir_pointer(orig_o);
	*dir_pointer(new_o) = *orig_dir;
	return dir_pointer(new_o);
}

static struct repo_dirent *repo_read_dirent(uint32_t revision, const uint32_t *path)
{
	uint32_t name = 0;
	struct repo_dirent *key = dent_pointer(dent_alloc(1));
	struct repo_dir *dir = NULL;
	struct repo_dirent *dent = NULL;
	dir = repo_commit_root_dir(commit_pointer(revision));
	while (~(name = *path++)) {
		key->name_offset = name;
		dent = dent_search(&dir->entries, key);
		if (dent == NULL || !repo_dirent_is_dir(dent))
			break;
		dir = repo_dir_from_dirent(dent);
	}
	dent_free(1);
	return dent;
}

static void repo_write_dirent(uint32_t *path, uint32_t mode,
			      uint32_t content_offset, uint32_t del)
{
	uint32_t name, revision, dir_o = ~0, parent_dir_o = ~0;
	struct repo_dir *dir;
	struct repo_dirent *key;
	struct repo_dirent *dent = NULL;
	revision = active_commit;
	dir = repo_commit_root_dir(commit_pointer(revision));
	dir = repo_clone_dir(dir);
	commit_pointer(revision)->root_dir_offset = dir_offset(dir);
	while (~(name = *path++)) {
		parent_dir_o = dir_offset(dir);

		key = dent_pointer(dent_alloc(1));
		key->name_offset = name;

		dent = dent_search(&dir->entries, key);
		if (dent == NULL)
			dent = key;
		else
			dent_free(1);

		if (dent == key) {
			dent->mode = REPO_MODE_DIR;
			dent->content_offset = 0;
			dent = dent_insert(&dir->entries, dent);
		}

		if (dent_offset(dent) < dent_pool.committed) {
			dir_o = repo_dirent_is_dir(dent) ?
					dent->content_offset : ~0;
			dent_remove(&dir->entries, dent);
			dent = dent_pointer(dent_alloc(1));
			dent->name_offset = name;
			dent->mode = REPO_MODE_DIR;
			dent->content_offset = dir_o;
			dent = dent_insert(&dir->entries, dent);
		}

		dir = repo_dir_from_dirent(dent);
		dir = repo_clone_dir(dir);
		dent->content_offset = dir_offset(dir);
	}
	if (dent == NULL)
		return;
	dent->mode = mode;
	dent->content_offset = content_offset;
	if (del && ~parent_dir_o)
		dent_remove(&dir_pointer(parent_dir_o)->entries, dent);
}

<<<<<<< HEAD
uint32_t repo_copy(uint32_t revision, uint32_t *src, uint32_t *dst)
=======
uint32_t repo_read_path(uint32_t *path)
{
	uint32_t content_offset = 0;
	struct repo_dirent *dent = repo_read_dirent(active_commit, path);
	if (dent != NULL)
		content_offset = dent->content_offset;
	return content_offset;
=======
	strbuf_reset(&buf);
	fast_export_ls(path, mode_out, &buf);
	return buf.buf;
>>>>>>> 7e69325... vcs-svn: eliminate repo_tree structure
}

<<<<<<< HEAD
uint32_t repo_read_mode(const char *path)
{
	uint32_t result;
	struct strbuf unused = STRBUF_INIT;

	fast_export_ls(path, &result, &unused);
	strbuf_release(&unused);
	return result;
}

<<<<<<< HEAD
void repo_copy(uint32_t revision, uint32_t *src, uint32_t *dst)
>>>>>>> efb4d0f... vcs-svn: simplify repo_modify_path and repo_copy
=======
=======
>>>>>>> fe5ffd8... vcs-svn: avoid using ls command twice
void repo_copy(uint32_t revision, const char *src, const char *dst)
>>>>>>> 01823f6... vcs-svn: pass paths through to fast-import
{
	uint32_t mode;
	struct strbuf data = STRBUF_INIT;

	fast_export_ls_rev(revision, src, &mode, &data);
	fast_export_modify(dst, mode, data.buf);
	strbuf_release(&data);
}

<<<<<<< HEAD
<<<<<<< HEAD
uint32_t repo_modify_path(uint32_t *path, uint32_t mode, uint32_t blob_mark)
=======
=======
>>>>>>> vcs-svn: pass paths through to fast-import
<<<<<<< HEAD
uint32_t repo_replace(uint32_t *path, uint32_t blob_mark)
>>>>>>> vcs-svn: simplify repo_modify_path and repo_copy
{
	struct repo_dirent *src_dent;
	src_dent = repo_read_dirent(active_commit, path);
	if (!src_dent)
		return 0;
	if (!blob_mark)
		blob_mark = src_dent->content_offset;
	if (!mode)
		mode = src_dent->mode;
	repo_write_dirent(path, mode, blob_mark, 0);
	return mode;
}

=======
>>>>>>> efb4d0f... vcs-svn: simplify repo_modify_path and repo_copy
void repo_delete(uint32_t *path)
=======
void repo_delete(const char *path)
>>>>>>> 01823f6... vcs-svn: pass paths through to fast-import
{
	fast_export_delete(path);
}
