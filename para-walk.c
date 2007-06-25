#include "cache.h"
#include "cache-tree.h"
#include "tree.h"
#include "tree-walk.h"
#include "para-walk.h"

static int init_para_iw(struct para_iw *walker)
{
	walker->pos = 0;
	walker->slash = 0;
	return 0;
}

static int extract_para_iw(struct para_iw *walker, struct para_walk_entry *e)
{
	struct cache_entry *ce;
	const char *slash;

	e->name = NULL;
	if (active_nr <= walker->pos)
		return WALKER_EOF;

	ce = active_cache[walker->pos];
	e->name = ce->name;

	/* walker->slash holds the position for the last directory
	 * returned as a fake tree entry from this index entry.
	 */
	slash = strchr(e->name + walker->slash, '/');

	if (!slash) {
		/* We have returned all the leading directories; return
		 * the blob entry itself.
		 */
		e->namelen = strlen(e->name);
		e->hash = ce->sha1;
		e->mode = canon_mode(ntohl(ce->ce_mode));
	}
	else {
		struct cache_tree *ctree;
		char pathbuf[PATH_MAX];

		/* Create a fake tree entry */
		e->namelen = slash - e->name;
		e->hash = null_sha1;
		e->mode = S_IFDIR;

		memcpy(pathbuf, e->name, e->namelen);
		pathbuf[e->namelen] = 0;
		ctree = cache_tree_find(active_cache_tree, pathbuf);
		if (ctree && 0 <= ctree->entry_count)
			e->hash = ctree->sha1;
	}
	return 0;
}

static int update_para_iw(struct para_iw *walker)
{
	struct cache_entry *ce;
	const char *slash;

	if (active_nr <= walker->pos)
		return WALKER_EOF;
	ce = active_cache[walker->pos];
	slash = strchr(ce->name + walker->slash, '/');
	if (slash)
		/* We still have subdirectory, so prepare to return it
		 * as the next fake tree entry.
		 */
		walker->slash = slash - ce->name + 1;
	else {
		struct cache_entry *ce2;
		int pos;

		walker->pos++;
		if (active_nr <= walker->pos)
			return WALKER_EOF;
		ce2 = active_cache[walker->pos];

		/* If we were looking at "a/b" and we are looking at
		 * "a/c" now, we have already returned "a/" while at
		 * the earlier index entry, so find the longest common
		 * leading path to prepare returning a new fake tree
		 * entry.
		 */
		slash = NULL;
		for (pos = 0;
		     ce2->name[pos] == ce->name[pos];
		     pos++)
			if (ce2->name[pos] == '/')
				slash = ce2->name + pos;
		if (slash)
			walker->slash = slash - ce2->name + 1;
		else
			walker->slash = 0;
	}
	return 0;
}

static int skip_para_iw(struct para_iw *walker, const char *path, int pathlen)
{
	int pos, len;
	const char *slash;
	struct cache_entry *ce;

	for (pos = walker->pos; pos < active_nr; pos++) {
		ce = active_cache[pos];
		len = ce_namelen(ce);
		if (len < (pathlen+1) ||
		    memcmp(path, ce->name, pathlen) ||
		    ce->name[pathlen] != '/')
			break;
	}
	/* pos points at the first one that is not under path */
	walker->pos = pos;
	if (active_nr <= pos)
		return WALKER_EOF;
	slash = NULL;
	ce = active_cache[pos];
	for (pos = 0; ce->name[pos] == path[pos]; pos++)
		if (ce->name[pos] == '/')
			slash = ce->name + pos;
	if (slash)
		walker->slash = slash - ce->name + 1;
	else
		walker->slash = 0;
	return 0;
}

static int init_para_tw(struct para_tw *walker, const unsigned char *hash)
{
	struct para_tw_rec *it;
	unsigned long size;

	it = xcalloc(1, sizeof(struct para_tw_rec));
	it->tree_buf = read_object_with_reference(hash, tree_type,
						  &size, NULL);
	if (!it->tree_buf) {
		free(it);
		return WALKER_ERR;
	}
	init_tree_desc(&it->tree_desc, it->tree_buf, size);
	walker->current = it;
	return 0;
}

static int extract_para_tw(struct para_tw *walker, struct para_walk_entry *e)
{
	struct para_tw_rec *r;
	int pos;

	e->name = NULL;
	if (!walker->current || !walker->current->tree_desc.size)
		return WALKER_EOF;

	/* Print the name recursively... */
	pos = sizeof(walker->namebuf) - 1;
	walker->namebuf[pos] = 0;
	for (r = walker->current; r; r = r->caller) {
		const unsigned char *hash;
		unsigned mode;
		const char *name;
		int len;

		hash = tree_entry_extract(&r->tree_desc, &name, &mode);
		len = strlen(name);
		if (pos < len)
			die("pathname too long");
		memcpy(walker->namebuf + pos - len, name, len);
		pos -= len;
		walker->namebuf[--pos] = '/';
		if (r == walker->current) {
			/* toplevel element */
			e->hash = hash;
			e->mode = mode;
		}
	}
	e->name = walker->namebuf + pos + 1;
	e->namelen = strlen(e->name);
	return 0;
}

static int update_para_tw(struct para_tw *walker)
{
	struct para_tw_rec *r;

	while (1) {
		r = walker->current;
		if (!r)
			return WALKER_EOF;
		if (!r->tree_desc.size) {
			/* We ran out the current desc so need to go back
			 * one level up.
			 */
			free(r->tree_buf);
			walker->current = r->caller;
			free(r);
			continue;
		}
		update_tree_entry(&r->tree_desc);
		if (r->tree_desc.size)
			break;
	}
	return 0;
}

/* This is similar to update_para_tw(), but does descend into
 * the tree when looking at one.
 */
static int descend_para_tw(struct para_tw *walker)
{
	const unsigned char *hash;
	const char *name_elem;
	unsigned mode;
	struct para_tw_rec *r, *it;
	unsigned long size;

	r = walker->current;
	hash = tree_entry_extract(&r->tree_desc, &name_elem, &mode);
	if (!S_ISDIR(mode))
		return update_para_tw(walker);

	it = xcalloc(1, sizeof(struct para_tw_rec));
	it->tree_buf = read_object_with_reference(hash, tree_type,
						  &size,
						  NULL);
	if (!it->tree_buf) {
		free(it);
		return WALKER_ERR;
	}
	init_tree_desc(&it->tree_desc, it->tree_buf, size);
	it->caller = walker->current;
	walker->current = it;
	return 0;
}

int init_para_walk(struct para_walk *w, const char **pathspec, int use_index, int use_worktree, int num_trees, unsigned char tree[][20])
{
	int i;

	if (!use_index && use_worktree)
		use_index = 1;
	memset(w, 0, sizeof(*w));
	w->pathspec = pathspec;
	w->use_index = use_index;
	w->use_worktree = use_worktree;
	w->num_trees = num_trees;
	w->tw = xcalloc(w->num_trees, sizeof(*w->tw));
	if (use_index)
		init_para_iw(&w->iw);
	for (i = 0; i < num_trees; i++)
		if (init_para_tw(&w->tw[i], tree[i]))
			return WALKER_ERR;
	/* peek[0] is for index tree, peek[1] is for working tree,
	 * and peek[2+i] is for i-th tree
	 */
	w->peek = xcalloc(w->num_trees + 2, sizeof(struct para_walk_entry));
	return 0;
}

/* Check if any working file path is dirty, stat-wise, under the given
 * hierarchy.  Right now nobody passes missing_ok but it is a
 * provision to work better in a sparsely populated working tree.
 */
static int dirty_subtree(const char *name, int len, int pos, int missing_ok)
{
	/* Scan cache starting at pos and see if there is any dirty
	 * file in the hierarchy under name.   Note that name does
	 * not end with a slash.
	 */
	for ( ; pos < active_nr; pos++) {
		struct stat st;
		struct cache_entry *ce = active_cache[pos];
		int l = ce_namelen(ce);
		if (l < (len+1) ||
		    memcmp(name, ce->name, len) ||
		    ce->name[len] != '/')
			break;

		/* ce is under directory "name" */
		if (!lstat(ce->name, &st)) {
			if (ce_match_stat(ce, &st, 0))
				return 1;
		}
		else if (missing_ok && errno == ENOENT)
			;
		else
			return 1;
	}
	return 0;
}

/* When walking working tree along with index, see if we know
 * that the given entry exactly matches the index, and copy the
 * result from the index in such a case.  This can even allow
 * us to skip the entire subdirectory if it is cache clean and
 * we have an up-to-date cache-tree structure for that hierarchy.
 */
static void fill_worktree_ent(struct para_walk *w)
{
	struct stat st;
	struct cache_entry *ce;
	char pathbuf[PATH_MAX];
	const char *path;
	struct para_walk_entry *e = &(w->peek[0]);

	ce = active_cache[w->iw.pos];
	e[1].hash = null_sha1; /* default to "unknown" */
	e[1].name = e->name;
	e[1].namelen = e->namelen;

	if (S_ISDIR(e->mode)) {
		/* a fake directory entry */
		path = pathbuf;
		memcpy(pathbuf, e->name, e->namelen);
		pathbuf[e->namelen] = 0;
	}
	else
		/* the blob itself is being returned */
		path = e->name;

	if (!lstat(path, &st)) {
		e[1].mode = canon_mode(st.st_mode);

		if (!S_ISDIR(st.st_mode)) {
			/* working tree object is a blob; if the index
			 * is returning a blob and the index entry is
			 * clean, then we know the object name for the
			 * working tree blob.
			 */
			if (!S_ISDIR(e->mode) && !ce_match_stat(ce, &st, 0))
				e[1].hash = e->hash;
			return;
		}

		/* working tree object is a directory; if we have a
		 * blob in the index with this name, we would never
		 * know the tree object name that would correspond to
		 * this directory in the working tree.
		 */
		if (!S_ISDIR(e->mode))
			return;

		/* we are returning a fake directory entry from the
		 * index and we are looking at a directory in the
		 * working tree; do we know the tree object name for
		 * this subtree via cache-tree mechanism?
		 */
		if (!is_null_sha1(e->hash)) {
			/* Yes.  If all the working tree blobs under this
			 * hierarchy are clean, then we can tell the entire
			 * subtree is identical to that of the index.
			 */
			if (!dirty_subtree(path, e->namelen, w->iw.pos, 0))
				e[1].hash = e->hash;
		}
	}
	else
		/* working tree does not have an entry here */
		e[1].name = NULL;
}

int extract_para_walk(struct para_walk *w)
{
	struct para_walk_entry *e;
	const char *first_name;
	unsigned first_mode, mask;
	int i, first_len;
	const char **spec = w->pathspec;

	for (i = 0; i < w->num_trees + 2; i++)
		memset(&(w->peek[i]), 0, sizeof(w->peek[i]));

	if (w->use_index) {
		e = &(w->peek[0]);
		while (!extract_para_iw(&w->iw, e)) {
			if (pathname_included(spec, e->name, e->namelen))
				break;
			update_para_iw(&w->iw);
		}
		if (e->name && w->use_worktree)
			fill_worktree_ent(w);
		else
			e[1].name = NULL;
	}

	for (i = 0; i < w->num_trees; i++) {
		e = &(w->peek[i+2]);
		while (!extract_para_tw(&w->tw[i], e)) {
			if (pathname_included(spec, e->name, e->namelen))
				break;
			/* This name is uninteresting; when it is a
			 * directory we can just skip it altogether.
			 * Nice.
			 */
			update_para_tw(&w->tw[i]);
		}
	}

	/* Find the earliest name */
	first_name = NULL;
	first_mode = first_len = 0;
	mask = 0;
	for (i = 0; i < w->num_trees + 2; i++) {
		e = &w->peek[i];
		if (!e->name)
			continue;
		if (first_name) {
			int j, l, cmp;

			l = (first_len < e->namelen) ? first_len : e->namelen;
			cmp = memcmp(first_name, e->name, l);
			if (cmp < 0) {
				/* e comes strictly later */
				e->name = NULL;
				continue;
			}
			if (cmp == 0 && (first_len == e->namelen)) {
				/* e is among the earliest ones */
				mask |= (1u<<i);
				/* we do this to avoid using the one from
				 * the index to be pointed at by first_name
				 */
				first_name = e->name;
				continue;
			}
			/* e comes strictly earlier; all the
			 * ones "earliest" so far are found not
			 * to be the earliest anymore.
			 */
			for (j = 0; j < i; j++) {
				if (mask & (1u<<j))
					w->peek[j].name = NULL;
			}
			mask = 0;
		}
		/* mark e among the earliest */
		mask |= (1u<<i);
		first_name = e->name;
		first_len = e->namelen;
		first_mode = e->mode;
	}
	if (!mask)
		return WALKER_EOF;
	if (first_name[first_len]) {
		/* first_name is a substring, i.e. a fake tree entry
		 * taken from the index.  The caller would want to use
		 * lstat() and such on the result so make it convenient
		 * for them.
		 */
		w->path = w->pathbuf;
		memcpy(w->pathbuf, first_name, first_len);
		w->pathbuf[first_len] = 0;
	}
	else
		/* first_name is pointing at a full string, not
		 * a substring so we can just reuse without memcpy().
		 */
		w->path = first_name;

	w->pathlen = first_len;

	for (i = 0; i < w->num_trees + 2; i++) {
		e = &w->peek[i];
		if (e->name)
			continue;
		e->mode = 0;
		e->name = w->path;
		e->namelen = w->pathlen;
		e->hash = null_sha1;
	}

	return 0;
}

void update_para_walk(struct para_walk *w)
{
	int i;

	for (i = 0; i < w->num_trees + 2; i++) {
		struct para_walk_entry *e;

		e = &w->peek[i];
		if (!e->mode)
			continue;
		switch (i) {
		case 0:
			/* index entry */
			update_para_iw(&w->iw);
			break;
		case 1:
			/* working tree */
			break;
		default:
			/* All others are trees, and the fact that
			 * this survived means this subtree is
			 * interesting.
			 */
			descend_para_tw(&w->tw[i-2]);
			break;
		}
	}
}

/* Advance walkers that are looking at the tree recorded at w->path */
void skip_para_walk(struct para_walk *w)
{
	int i;

	for (i = 0; i < w->num_trees + 2; i++) {
		struct para_walk_entry *e;

		e = &w->peek[i];
		if (!e->mode)
			continue;
		switch (i) {
		case 0:
			/* index entry */
			skip_para_iw(&w->iw, w->path, w->pathlen);
			break;
		case 1:
			/* working tree */
			break;
		default:
			update_para_tw(&w->tw[i-2]);
			break;
		}
	}
}
