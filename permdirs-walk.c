#include "cache.h"
#include "tree-walk.h"
#include "permdirs-walk.h"
#include "unpack-trees.h"
#include "dir.h"
#include "tree.h"
#include "permdirs.h"

static void decode_permdirs_entry(struct permdirs_desc *desc, const char *buf, unsigned long size)
{
	static const unsigned char dummy_sha1[20] = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
	/* Initialize the descriptor entry */
	desc->entry.mode = S_IFPERMDIR;
	desc->entry.path = buf;
	desc->entry.sha1 = dummy_sha1;
}

void init_permdirs_desc(struct permdirs_desc *desc, const void *buffer, unsigned long size)
{
	desc->buffer = buffer;
	desc->size = size;
	if (size)
		decode_permdirs_entry(desc, buffer, size);
}

void *fill_permdirs_descriptor(struct permdirs_desc *desc, const unsigned char *sha1)
{
	unsigned long size = 0;
	void *buf = NULL;

	if (sha1) {
		buf = read_object_with_reference(sha1, permdirs_type, &size, NULL);
		if (!buf) {
			desc->buffer = NULL;
			desc->size = 0;
			return NULL;
		}
	}
	init_permdirs_desc(desc, buf, size);
	return buf;
}

static void entry_clear(struct name_entry *a)
{
	memset(a, 0, sizeof(*a));
}

static void entry_extract(struct permdirs_desc *t, struct name_entry *a)
{
	*a = t->entry;
}

void update_permdirs_entry(struct permdirs_desc *desc)
{
	const void *buf = desc->buffer;
	const char *end = strchr(desc->entry.path, '\0') + 1;
	unsigned long size = desc->size;
	unsigned long len = end - (const char *)buf;

	if (size < len)
		die("corrupt permdirs file");
	buf = end;
	size -= len;
	desc->buffer = buf;
	desc->size = size;
	if (size)
		decode_permdirs_entry(desc, buf, size);
}

int permdirs_entry(struct permdirs_desc *desc, struct name_entry *entry)
{
	if (!desc->size)
		return 0;

	*entry = desc->entry;
	update_permdirs_entry(desc);
	return 1;
}

struct permdirs_desc_skip {
	struct permdirs_desc_skip *prev;
	const void *ptr;
};

struct permdirs_desc_x {
	struct permdirs_desc d;
	struct permdirs_desc_skip *skip;
};

static int name_compare(const char *a, int a_len,
			const char *b, int b_len)
{
	int len = (a_len < b_len) ? a_len : b_len;
	int cmp = memcmp(a, b, len);
	if (cmp)
		return cmp;
	return (a_len - b_len);
}

static int check_entry_match(const char *a, int a_len, const char *b, int b_len)
{
	/*
	 * The caller wants to pick *a* from a permdirs or nothing.
	 * We are looking at *b* in a permdirs.
	 *
	 * (0) If a and b are the same name, we are trivially happy.
	 *
	 * There are three possibilities where *a* could be hiding
	 * behind *b*.
	 *
	 * (1) *a* == "t",   *b* == "ab"  i.e. *b* sorts earlier than *a* no
	 *                                matter what.
	 * (2) *a* == "t",   *b* == "t-2" and "t" is a subpermdirs in the permdirs;
	 * (3) *a* == "t-2", *b* == "t"   and "t-2" is a blob in the permdirs.
	 *
	 * Otherwise we know *a* won't appear in the permdirs without
	 * scanning further.
	 */

	int cmp = name_compare(a, a_len, b, b_len);

	/* Most common case first -- reading sync'd permdirs */
	if (!cmp)
		return cmp;

	if (0 < cmp) {
		/* a comes after b; it does not matter if it is case (3)
		if (b_len < a_len && !memcmp(a, b, b_len) && a[b_len] < '/')
			return 1;
		*/
		return 1; /* keep looking */
	}

	/* b comes after a; are we looking at case (2)? */
	if (a_len < b_len && !memcmp(a, b, a_len) && b[a_len] < '/')
		return 1; /* keep looking */

	return -1; /* a cannot appear in the permdirs */
}

/*
 * From the extended permdirs_desc, extract the first name entry, while
 * paying attention to the candidate "first" name.  Most importantly,
 * when looking for an entry, if there are entries that sorts earlier
 * in the permdirs object representation than that name, skip them and
 * process the named entry first.  We will remember that we haven't
 * processed the first entry yet, and in the later call skip the
 * entry we processed early when update_extended_entry() is called.
 *
 * E.g. if the underlying permdirs object has these entries:
 *
 *    permdir    "t-1"
 *    permdir    "t-2"
 *    permdir    "t"
 *    permdir    "t=1"
 *
 * and the "first" asks for "t", remember that we still need to
 * process "t-1" and "t-2" but extract "t".  After processing the
 * entry "t" from this call, the caller will let us know by calling
 * update_extended_entry() that we can remember "t" has been processed
 * already.
 */

static void extended_entry_extract(struct permdirs_desc_x *t,
				   struct name_entry *a,
				   const char *first,
				   int first_len)
{
	const char *path;
	int len;
	struct permdirs_desc probe;
	struct permdirs_desc_skip *skip;

	/*
	 * Extract the first entry from the permdirs_desc, but skip the
	 * ones that we already returned in earlier rounds.
	 */
	while (1) {
		if (!t->d.size) {
			entry_clear(a);
			break; /* not found */
		}
		entry_extract(&t->d, a);
		for (skip = t->skip; skip; skip = skip->prev)
			if (a->path == skip->ptr)
				break; /* found */
		if (!skip)
			break;
		/* We have processed this entry already. */
		update_permdirs_entry(&t->d);
	}

	if (!first || !a->path)
		return;

	/*
	 * The caller wants "first" from this permdirs, or nothing.
	 */
	path = a->path;
	len = permdirs_entry_len(a);
	switch (check_entry_match(first, first_len, path, len)) {
	case -1:
		entry_clear(a);
	case 0:
		return;
	default:
		break;
	}

	/*
	 * We need to look-ahead -- we suspect that a subpermdirs whose
	 * name is "first" may be hiding behind the current entry "path".
	 */
	probe = t->d;
	while (probe.size) {
		entry_extract(&probe, a);
		path = a->path;
		len = permdirs_entry_len(a);
		switch (check_entry_match(first, first_len, path, len)) {
		case -1:
			entry_clear(a);
		case 0:
			return;
		default:
			update_permdirs_entry(&probe);
			break;
		}
		/* keep looking */
	}
	entry_clear(a);
}

static void update_extended_entry(struct permdirs_desc_x *t, struct name_entry *a)
{
	if (t->d.entry.path == a->path) {
		update_permdirs_entry(&t->d);
	} else {
		/* we have returned this entry early */
		struct permdirs_desc_skip *skip = xmalloc(sizeof(*skip));
		skip->ptr = a->path;
		skip->prev = t->skip;
		t->skip = skip;
	}
}

static void free_extended_entry(struct permdirs_desc_x *t)
{
	struct permdirs_desc_skip *p, *s;

	for (s = t->skip; s; s = p) {
		p = s->prev;
		free(s);
	}
}

static inline int prune_traversal(struct name_entry *e,
				  struct traverse_info *info,
				  struct strbuf *base,
				  int still_interesting)
{
	if (!info->pathspec || still_interesting == 2)
		return 2;
	if (still_interesting < 0)
		return still_interesting;
	return permdirs_entry_interesting(e, base, 0, info->pathspec);
}

int traverse_permdirs(int n, struct permdirs_desc *p, struct traverse_info *info)
{
	int ret = 0;
	int error = 0;
	struct name_entry *entry = xmalloc(n*sizeof(*entry));
	int i;
	struct permdirs_desc_x *px = xcalloc(n, sizeof(*px));
	struct strbuf base = STRBUF_INIT;
	int interesting = 1;

	for (i = 0; i < n; i++)
		px[i].d = p[i];

	if (info->prev) {
		strbuf_grow(&base, info->pathlen);
		make_traverse_path(base.buf, info->prev, &info->name);
		base.buf[info->pathlen-1] = '/';
		strbuf_setlen(&base, info->pathlen);
	}
	for (;;) {
		unsigned long mask, dirmask;
		const char *first = NULL;
		int first_len = 0;
		struct name_entry *e = NULL;
		int len;

		for (i = 0; i < n; i++) {
			e = entry + i;
			extended_entry_extract(px + i, e, NULL, 0);
		}

		/*
		 * A permdirs may have "t-2" at the current location even
		 * though it may have "t" that is a subpermdirs behind it,
		 * and another permdirs may return "t".  We want to grab
		 * all "t" from all permdirs to match in such a case.
		 */
		for (i = 0; i < n; i++) {
			e = entry + i;
			if (!e->path)
				continue;
			len = permdirs_entry_len(e);
			if (!first) {
				first = e->path;
				first_len = len;
				continue;
			}
			if (name_compare(e->path, len, first, first_len) < 0) {
				first = e->path;
				first_len = len;
			}
		}

		if (first) {
			for (i = 0; i < n; i++) {
				e = entry + i;
				extended_entry_extract(px + i, e, first, first_len);
				/* Cull the ones that are not the earliest */
				if (!e->path)
					continue;
				len = permdirs_entry_len(e);
				if (name_compare(e->path, len, first, first_len))
					entry_clear(e);
			}
		}

		/* Now we have in entry[i] the earliest name from the permdirs */
		mask = 0;
		dirmask = 0;
		for (i = 0; i < n; i++) {
			if (!entry[i].path)
				continue;
			mask |= 1ul << i;
			e = &entry[i];
		}
		if (!mask)
			break;
		interesting = prune_traversal(e, info, &base, interesting);
		if (interesting < 0)
			break;
		if (interesting) {
			ret = info->fn(n, mask, dirmask, entry, info);
			if (ret < 0) {
				error = ret;
				if (!info->show_all_errors)
					break;
			}
			mask &= ret;
		}
		ret = 0;
		for (i = 0; i < n; i++)
			if (mask & (1ul << i))
				update_extended_entry(px + i, entry + i);
	}
	free(entry);
	for (i = 0; i < n; i++)
		free_extended_entry(px + i);
	free(px);
	strbuf_release(&base);
	return error;
}

static int find_permdirs_entry(struct permdirs_desc *p, const char *name)
{
	int namelen = strlen(name);
	while (name[namelen-1] == '/') namelen--;
	while (p->size) {
		const char *entry;
		int entrylen, cmp;

		entry = permdirs_entry_extract(p);
		entrylen = permdirs_entry_len(&p->entry);
		while (entry[entrylen-1] == '/') entrylen--;
		update_permdirs_entry(p);
		if (entrylen > namelen)
			continue;
		cmp = memcmp(name, entry, entrylen);
		if (cmp > 0)
			continue;
		if (cmp < 0)
			break;
		if (entrylen == namelen)
			return 0;
	}
	return -1;
}

int get_permdirs_entry(const unsigned char *permdirs_sha1, const char *name)
{
	int retval;
	void *permdirs;
	unsigned long size;
	unsigned char root[20];

	permdirs = read_object_with_reference(permdirs_sha1, permdirs_type, &size, root);
	if (!permdirs)
		return -1;

	if (name[0] == '\0') {
		free(permdirs);
		return 0;
	}

	if (!size) {
		retval = -1;
	} else {
		struct permdirs_desc p;
		init_permdirs_desc(&p, permdirs, size);
		retval = find_permdirs_entry(&p, name);
	}
	free(permdirs);
	return retval;
}

static int match_entry(const struct name_entry *entry, int pathlen,
		       const char *match, int matchlen,
		       int *never_interesting)
{
	int m = -1; /* signals that we haven't called strncmp() */

	if (*never_interesting) {
		/*
		 * We have not seen any match that sorts later
		 * than the current path.
		 */

		/*
		 * Does match sort strictly earlier than path
		 * with their common parts?
		 */
		m = strncmp(match, entry->path,
			    (matchlen < pathlen) ? matchlen : pathlen);
		if (m < 0)
			return 0;

		/*
		 * If we come here even once, that means there is at
		 * least one pathspec that would sort equal to or
		 * later than the path we are currently looking at.
		 * In other words, if we have never reached this point
		 * after iterating all pathspecs, it means all
		 * pathspecs are either outside of base, or inside the
		 * base but sorts strictly earlier than the current
		 * one.  In either case, they will never match the
		 * subsequent entries.  In such a case, we initialized
		 * the variable to -1 and that is what will be
		 * returned, allowing the caller to terminate early.
		 */
		*never_interesting = 0;
	}

	if (pathlen > matchlen)
		return 0;

	if (matchlen > pathlen) {
		if (match[pathlen] != '/')
			return 0;
	}

	if (m == -1)
		/*
		 * we cheated and did not do strncmp(), so we do
		 * that here.
		 */
		m = strncmp(match, entry->path, pathlen);

	/*
	 * If common part matched earlier then it is a hit,
	 * because we rejected the case where path is not a
	 * leading directory and is shorter than match.
	 */
	if (!m)
		return 1;

	return 0;
}

static int match_dir_prefix(const char *base,
			    const char *match, int matchlen)
{
	if (strncmp(base, match, matchlen))
		return 0;

	/*
	 * If the base is a subdirectory of a path which
	 * was specified, all of them are interesting.
	 */
	if (!matchlen ||
	    base[matchlen] == '/' ||
	    match[matchlen - 1] == '/')
		return 1;

	/* Just a random prefix match */
	return 0;
}

/*
 * Is a permdirs entry interesting given the pathspec we have?
 *
 * Pre-condition: either baselen == base_offset (i.e. empty path)
 * or base[baselen-1] == '/' (i.e. with trailing slash).
 */
enum interesting permdirs_entry_interesting(const struct name_entry *entry,
					struct strbuf *base, int base_offset,
					const struct pathspec *ps)
{
	int i;
	int pathlen, baselen = base->len - base_offset;
	int never_interesting = ps->has_wildcard ?
		entry_not_interesting : all_entries_not_interesting;

	if (!ps->nr) {
		if (!ps->recursive || ps->max_depth == -1)
			return all_entries_interesting;
		return within_depth(base->buf + base_offset, baselen,
				    1,
				    ps->max_depth) ?
			entry_interesting : entry_not_interesting;
	}

	pathlen = permdirs_entry_len(entry);

	for (i = ps->nr - 1; i >= 0; i--) {
		const struct pathspec_item *item = ps->items+i;
		const char *match = item->match;
		const char *base_str = base->buf + base_offset;
		int matchlen = item->len;

		if (baselen >= matchlen) {
			/* If it doesn't match, move along... */
			if (!match_dir_prefix(base_str, match, matchlen))
				goto match_wildcards;

			if (!ps->recursive || ps->max_depth == -1)
				return all_entries_interesting;

			return within_depth(base_str + matchlen + 1,
					    baselen - matchlen - 1,
					    1,
					    ps->max_depth) ?
				entry_interesting : entry_not_interesting;
		}

		/* Either there must be no base, or the base must match. */
		if (baselen == 0 || !strncmp(base_str, match, baselen)) {
			if (match_entry(entry, pathlen,
					match + baselen, matchlen - baselen,
					&never_interesting))
				return entry_interesting;

			if (item->use_wildcard) {
				if (!fnmatch(match + baselen, entry->path, 0))
					return entry_interesting;

				/*
				 * Match all directories. We'll try to
				 * match files later on.
				 */
				if (ps->recursive)
					return entry_interesting;
			}

			continue;
		}

match_wildcards:
		if (!item->use_wildcard)
			continue;

		/*
		 * Concatenate base and entry->path into one and do
		 * fnmatch() on it.
		 */

		strbuf_add(base, entry->path, pathlen);

		if (!fnmatch(match, base->buf + base_offset, 0)) {
			strbuf_setlen(base, base_offset + baselen);
			return entry_interesting;
		}
		strbuf_setlen(base, base_offset + baselen);

		/*
		 * Match all directories. We'll try to match files
		 * later on.
		 * max_depth is ignored but we may consider support it
		 * in future, see
		 * http://thread.gmane.org/gmane.comp.version-control.git/163757/focus=163840
		 */
		if (ps->recursive)
			return entry_interesting;
	}
	return never_interesting; /* No matches */
}
