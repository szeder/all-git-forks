/*
 * Helper file for mountain of SVN callbacks in "editing".
 *
 * In SVN history replay, each revision is applied as a series of functions
 * (something like a SAX XML parser) with some associated data. This file
 * implements a bunch of callbacks to serialize that mess into a logical object
 * that can be applied on a Git branch.
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

#include <svn_client.h>
#include <svn_cmdline.h>
#include <svn_delta.h>
#include <svn_hash.h>
#include <svn_path.h>
#include <svn_props.h>
#include <svn_pools.h>
#include <svn_ra.h>
#include <svn_repos.h>
#include <svn_string.h>

#define NO_THE_INDEX_COMPATIBILITY_MACROS

#include "builtin.h"
#include "cache.h"
#include "dir.h"
#include "parse-options.h"
#include "remote.h"
#include "thread-utils.h"
#include "transport.h"
#include "help.h"

#include "isvn/isvn-internal.h"

struct dir_baton {
	struct branch_rev *br;
	char path[0];
};

bool path_startswith(const char *haystack, const char *needle)
{
	size_t len = strlen(needle);

	/* /foo, /bar -> no */
	if (strncmp(haystack, needle, len) != 0)
		return false;

	/* /foo, /foo -> yes; /foo/bar, /foo -> yes */
	if (haystack[len] == '\0' || haystack[len] == '/')
		return true;

	/* /foobar, /foo -> no */
	return false;
}

static const char *g_repo_pre;
static void strip_repo_init(void)
{
	const char *pre;

	if (strcmp(g_svn_url, g_repos_root) == 0) {
		g_repo_pre = strintern("");
		return;
	}

	/* INVARIANTS */
	if (strlen(g_svn_url) < strlen(g_repos_root))
		die("%s: url above root?", __func__);

	/* INVARIANTS */
	if (strncmp(g_svn_url, g_repos_root, strlen(g_repos_root)) != 0)
		die("%s: url not below root? '%s' of '%s'", __func__,
			g_svn_url, g_repos_root);

	pre = g_svn_url + strlen(g_repos_root);
	if (*pre == '/')
		pre++;

	g_repo_pre = strintern(pre);
}

static const char *strip_repo_pre(const char *path)
{
	if (*path == '/')
		path++;

	if (path_startswith(path, g_repo_pre))
		path += strlen(g_repo_pre);

	return path;
}

/* Ignore some directories. E.g. googlecode has:
 *
 * trunk/
 * branches/_?_/
 * tags/_?_/
 * wiki/
 *
 * Valid branches include trunk/, branches/<foo>, tags/<foo>. But not wiki/.
 *
 * Returns a pointer into 'path', skipping the branch prefix. If 'branch_out'
 * isn't NULL, *branch_out is set to an interned string of the branch.
 */
const char *strip_branch(const char *path, const char **branch_out)
{
	const char *end;

	path = strip_repo_pre(path);

	if (*path == '/')
		path++;

	if (path_startswith(path, option_branches))
		end = strchrnul(path + strlen(option_branches) + 1, '/');
	else if (path_startswith(path, option_tags))
		end = strchrnul(path + strlen(option_tags) + 1, '/');
	else if (path_startswith(path, option_trunk))
		end = strchrnul(path, '/');
	else
		die("%s: Not on a branch? '%s'", __func__, path);

	if (branch_out)
		*branch_out = memintern(path, end - path);

	if (*end)
		/* path = <branch> / <...>
		 *                 ^ end */
		return end + 1;

	/* path = <branch> \0
	 *                  ^ end */
	return end;
}

/* rev_branch:
 *
 * Kind of a mess. Set / invariants this revision's branch, if the path is on a
 * tracked branch.
 *
 * Return true if the path is on a tracked branch (i.e., not GoogleCode's
 * "/wiki").
 */
static bool rev_branch(struct branch_rev **br_inout, const char *path)
{
	struct branch_rev *br, *br2;
	const char *end = NULL;
	size_t branch_len;

	br = *br_inout;
	path = strip_repo_pre(path);

	if (*path == '/')
		path++;

	if (path_startswith(path, option_branches))
		end = strchr(path + strlen(option_branches) + 1, '/');
	else if (path_startswith(path, option_tags))
		end = strchr(path + strlen(option_tags) + 1, '/');
	else if (path_startswith(path, option_trunk))
		end = strchr(path, '/');
	else
		return false;

	if (end)
		branch_len = end - path;
	else
		branch_len = strlen(path);

	if (br->rv_branch == NULL) {
		br->rv_branch = memintern(path, branch_len);
		return true;
	}

	/* If we think this is branch A already, and this edit is associated
	 * with a different branch B, well...
	 *
	 * Create another seperate branch_rev for B at the same revision. */
	if (!br->rv_only_empty_dirs && (strlen(br->rv_branch) != branch_len ||
	    memcmp(br->rv_branch, path, branch_len) != 0)) {

		br2 = new_branch_rev(br->rv_rev);
		br2->rv_author = xstrdup(br->rv_author);
		br2->rv_logmsg = xstrdup(br->rv_logmsg);
		br2->rv_timestamp = br->rv_timestamp;
		br2->rv_branch = memintern(path, branch_len);
		/* Don't insert into revmap. */
		br2->rv_secondary = true;

		br->rv_affil = br2;

		*br_inout = br2;

		if (option_verbosity > 1)
			printf("W: non-empty r%u touches multiple branches: %s, %s!\n",
			    br->rv_rev, br->rv_branch, br2->rv_branch);
	}

	return true;
}

static struct br_edit *get_edit(struct branch_rev *br, const char *path)
{
	struct br_edit edlook;

	edlook.e_path = __DECONST(path, char *);
	hashmap_entry_init(&edlook.e_entry, strhash(path));

	return hashmap_get(&br->rv_edits, &edlook, NULL);
}

static struct br_edit *mk_edit(struct branch_rev *br, const char *path,
	enum ed_type ed, bool deleted)
{
	struct br_edit *edit, edlook;

	/* Not a branch we're following? */
	if (!rev_branch(&br, path)) {
		/* Ignore. */
		return NULL;
	}

	if (ed != ED_NIL && ed != ED_MKDIR)
		br->rv_only_empty_dirs = false;
	if (deleted)
		br->rv_only_empty_dirs = false;

	edlook.e_path = __DECONST(path, char *);
	hashmap_entry_init(&edlook.e_entry, strhash(path));

	edit = hashmap_get(&br->rv_edits, &edlook, NULL);
	if (edit) {
		/* INVARIANTS */
		if (deleted)
			die("Bogus modify-then-delete!");
		/* In general, setting a second edit type is NOT ok. */
		if (ed != ED_NIL && (edit->e_kind & ED_TYPEMASK) != 0) {
			/* However, addfile + textdelta is common. */
			if ((ed == ED_TEXTDELTA && (edit->e_kind & ED_TYPEMASK) ==
				ED_ADDFILE) ||
			    /* Or, add file/directory + props. */
			    (ed == ED_PROP && ((edit->e_kind & ED_TYPEMASK) ==
					ED_ADDFILE || (edit->e_kind & ED_TYPEMASK)
					== ED_MKDIR)))
				/* fine */;
			else
				die("Multiple edit types!");
		}

		/* Set new kind */
		if ((edit->e_kind & ED_TYPEMASK) == ED_NIL && ed != ED_NIL)
			edit->e_kind |= ed;
	} else {
		/* INVARIANTS */
		if (deleted && ed != ED_NIL)
			die("Bogus delete-and-type");

		/* Create new entry, initialize, and add to hash */
		edit = xmalloc(sizeof(*edit));
		if (edit == NULL)
			die("malloc");

		memset(edit, 0, sizeof(*edit));
		edit->e_path = xstrdup(path);
		if (deleted)
			edit->e_kind = ED_DELETE;
		else
			edit->e_kind = ed;

		hashmap_entry_init(&edit->e_entry, strhash(path));
		hashmap_add(&br->rv_edits, edit);
		TAILQ_INSERT_TAIL(&br->rv_editorder, edit, e_list);
	}

	return edit;
}

/* This API sucks. Why is open_root != open_directory? Why isn't edit_baton
 * passed to all methods ?!?! */
static svn_error_t *open_root(void *edit_data, svn_revnum_t dummy1 __unused,
	apr_pool_t *dummy2 __unused, void **dir_data)
{
	struct branch_rev *br;
	struct dir_baton *db;

	br = edit_data;

	db = xmalloc(sizeof(struct dir_baton) + 1);
	db->br = br;
	db->path[0] = '\0';

	*dir_data = db;

	return NULL;
}

static svn_error_t *open_(const char *path, void *parent_baton,
	svn_revnum_t base_revision, apr_pool_t *dummy __unused,
	void **child_baton)
{
	struct dir_baton *pdb = parent_baton, *db;

	db = xmalloc(sizeof(struct dir_baton) + strlen(path) + 1);
	db->br = pdb->br;
	strcpy(db->path, path);

	*child_baton = db;

	return NULL;
}

svn_error_t *_add_internal(const char *path, void *parent_baton,
	const char *copyfrom_path, svn_revnum_t copyfrom_revision,
	void **child_baton, bool directory)
{
	struct dir_baton *db;
	struct br_edit *edit;
	svn_error_t *err;

	err = open_(path, parent_baton, 0, NULL, child_baton);
	if (err)
		return err;

	db = *child_baton;

	edit = mk_edit(db->br, path, directory? ED_MKDIR : ED_ADDFILE, false);
	if (edit == NULL)
		return NULL;

	if (copyfrom_path) {
		/* Special case for ADD of non-empty directories. */
		db->br->rv_only_empty_dirs = false;

		edit->e_copyfrom = xstrdup(copyfrom_path);
		edit->e_copyrev = copyfrom_revision;

		if (option_verbosity > 2)
			fprintf(stderr, "C\t%s@%ju ->\t%s%s\n", copyfrom_path,
				(uintmax_t)copyfrom_revision, db->path,
				directory? "/" : "");
	} else {
		if (option_verbosity > 2)
			fprintf(stderr, "A\t%s%s\n", db->path, directory? "/" : "");
	}

	return NULL;
}

svn_error_t *add_file(const char *path, void *parent_baton,
	const char *copyfrom_path, svn_revnum_t copyfrom_revision,
	apr_pool_t *dummy __unused, void **child_baton)
{
	return _add_internal(path, parent_baton, copyfrom_path,
		copyfrom_revision, child_baton, false);
}

svn_error_t *add_directory(const char *path, void *parent_baton,
	const char *copyfrom_path, svn_revnum_t copyfrom_revision,
	apr_pool_t *dummy __unused, void **child_baton)
{
	return _add_internal(path, parent_baton, copyfrom_path,
		copyfrom_revision, child_baton, true);
}

static svn_error_t *close_directory(void *dir_baton, apr_pool_t *dummy __unused)
{
	struct dir_baton *db = dir_baton;

	if (db)
		free(db);
	else
		printf("XXX %s(NULL) !!!\n", __func__);

	return NULL;
}

static svn_error_t *close_file(void *file_baton, const char *text_checksum,
	apr_pool_t *dummy __unused)
{
	struct dir_baton *db = file_baton;
	struct br_edit *edit;
	int rc;

	if (db == NULL) {
		printf("XXX %s(NULL) !!!\n", __func__);
		return NULL;
	}

	if (text_checksum) {
		edit = get_edit(db->br, db->path);
		if (edit) {
			rc = hex_to_bin(text_checksum, edit->e_postimage_md5,
				16);
			if (rc < 0)
				die("%s: invalid hex string: %.*s\n", __func__,
					32, text_checksum);
		}
	}

	free(db);
	return NULL;
}

static svn_error_t *delete_entry(const char *path, svn_revnum_t revision,
	void *parent_baton, apr_pool_t *dummy __unused)
{
	struct dir_baton *db = parent_baton;

	if (mk_edit(db->br, path, ED_NIL, true/*deleted*/) == NULL)
		return NULL;

	if (option_verbosity > 2)
		fprintf(stderr, "D\t%s\n", path);

	return NULL;
}

static svn_error_t *absent(const char *path, void *parent_baton,
	apr_pool_t *dummy __unused)
{
	struct dir_baton *db = parent_baton;

	die("Got absent file/directory (%s/%s), do you have permissions to "
	    "fetch this repository?", db->path, path);
	/* NORETURN */
}

static svn_error_t *diffwrite(void *baton, const char *data, apr_size_t *len)
{
	struct br_edit *edit;

	if (*len == 0)
		return NULL;

	edit = baton;

	edit->e_diff = xrealloc(edit->e_diff, edit->e_difflen + *len);
	memcpy(edit->e_diff + edit->e_difflen, data, *len);

	edit->e_difflen += *len;
	return NULL;
}

static svn_error_t *apply_textdelta(void *file_baton,
	const char *base_checksum, apr_pool_t *pool,
	svn_txdelta_window_handler_t *handler, void **handler_baton)
{
	svn_stream_t *diffstream;
	struct dir_baton *db;
	struct br_edit *edit;
	int rc;

	db = file_baton;
	edit = mk_edit(db->br, db->path, ED_TEXTDELTA, false);
	if (edit == NULL) {
		/* For paths outside of branches, who cares? */
		*handler = svn_delta_noop_window_handler;
		return NULL;
	}

	if (base_checksum) {
		rc = hex_to_bin(base_checksum, edit->e_preimage_md5, 16);
		if (rc < 0)
			die("%s: invalid hex string: %.*s\n", __func__, 32,
				base_checksum);
	}

	diffstream = svn_stream_create(edit, pool);
	svn_stream_set_write(diffstream, diffwrite);

	/* Call. Back. Hell.
	 * This just sets up the callbacks so that eventually, diffwrite(edit)
	 * will be invoked. */
	svn_txdelta_to_svndiff3(handler, handler_baton, diffstream, 0,
		SVN_DELTA_COMPRESSION_LEVEL_DEFAULT, pool);

	if (option_verbosity > 2)
		fprintf(stderr, "M\t%s\n", db->path);

	return NULL;
}

static svn_error_t *change_prop(void *baton, const char *name,
	const svn_string_t *value, apr_pool_t *dummy __unused)
{
	struct dir_baton *db = baton;

	/* Do we care about file properties ? */
#if 0
	struct br_edit *edit;

	edit = mk_edit(db->br, db->path, ED_PROP, false);
	if (edit == NULL)
		return NULL;
#endif

	if (option_verbosity > 2)
		fprintf(stderr, "M\t%s\n", db->path);

	return NULL;
}

void isvn_editor_inialize_dedit_obj(svn_delta_editor_t *de)
{
	de->absent_directory = absent;
	de->absent_file = absent;

	de->open_root = open_root;
	de->open_directory = open_;
	de->open_file = open_;
	de->close_directory = close_directory;
	de->close_file = close_file;

	de->add_directory = add_directory;
	de->add_file = add_file;
	de->delete_entry = delete_entry;
	de->apply_textdelta = apply_textdelta;

	de->change_dir_prop = change_prop;
	de->change_file_prop = change_prop;
}

void isvn_editor_init(void)
{
	strip_repo_init();
}
