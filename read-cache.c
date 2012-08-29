/*
 * GIT - The information manager from hell
 *
 * Copyright (C) Linus Torvalds, 2005
 */
#define NO_THE_INDEX_COMPATIBILITY_MACROS
#include "cache.h"
#include "read-cache.h"
#include "cache-tree.h"
#include "refs.h"
#include "dir.h"
#include "tree.h"
#include "commit.h"
#include "blob.h"
#include "resolve-undo.h"
#include "strbuf.h"
#include "varint.h"

static struct cache_entry *refresh_cache_entry(struct cache_entry *ce, int really);

struct index_state the_index;

void set_index_entry(struct index_state *istate, int nr, struct cache_entry *ce)
{
	istate->cache[nr] = ce;
	add_name_hash(istate, ce);
}

static void replace_index_entry(struct index_state *istate, int nr, struct cache_entry *ce)
{
	struct cache_entry *old = istate->cache[nr];

	remove_name_hash(old);
	set_index_entry(istate, nr, ce);
	istate->cache_changed = 1;
}

void rename_index_entry_at(struct index_state *istate, int nr, const char *new_name)
{
	struct cache_entry *old = istate->cache[nr], *new;
	int namelen = strlen(new_name);

	new = xmalloc(cache_entry_size(namelen));
	copy_cache_entry(new, old);
	new->ce_flags &= ~CE_STATE_MASK;
	new->ce_namelen = namelen;
	memcpy(new->name, new_name, namelen + 1);

	cache_tree_invalidate_path(istate->cache_tree, old->name);
	remove_index_entry_at(istate, nr);
	add_index_entry(istate, new, ADD_CACHE_OK_TO_ADD|ADD_CACHE_OK_TO_REPLACE);
}

uint32_t calculate_stat_crc(struct cache_entry *ce)
{
	unsigned int ctimens = 0;
	uint32_t stat, stat_crc;

	stat = htonl(ce->ce_ctime.sec);
	stat_crc = crc32(0, (Bytef*)&stat, 4);
#ifdef USE_NSEC
	ctimens = ce->ce_ctime.nsec;
#endif
	stat = htonl(ctimens);
	stat_crc = crc32(stat_crc, (Bytef*)&stat, 4);
	stat = htonl(ce->ce_ino);
	stat_crc = crc32(stat_crc, (Bytef*)&stat, 4);
	stat = htonl(ce->ce_dev);
	stat_crc = crc32(stat_crc, (Bytef*)&stat, 4);
	stat = htonl(ce->ce_uid);
	stat_crc = crc32(stat_crc, (Bytef*)&stat, 4);
	stat = htonl(ce->ce_gid);
	stat_crc = crc32(stat_crc, (Bytef*)&stat, 4);
	return stat_crc;
}

/*
 * This only updates the "non-critical" parts of the directory
 * cache, ie the parts that aren't tracked by GIT, and only used
 * to validate the cache.
 */
void fill_stat_cache_info(struct cache_entry *ce, struct stat *st)
{
	ce->ce_ctime.sec = (unsigned int)st->st_ctime;
	ce->ce_mtime.sec = (unsigned int)st->st_mtime;
	ce->ce_ctime.nsec = ST_CTIME_NSEC(*st);
	ce->ce_mtime.nsec = ST_MTIME_NSEC(*st);
	ce->ce_dev = st->st_dev;
	ce->ce_ino = st->st_ino;
	ce->ce_uid = st->st_uid;
	ce->ce_gid = st->st_gid;
	ce->ce_size = st->st_size;

	if (assume_unchanged)
		ce->ce_flags |= CE_VALID;

	if (S_ISREG(st->st_mode))
		ce_mark_uptodate(ce);

	ce->ce_stat_crc = calculate_stat_crc(ce);
}

static int ce_compare_data(struct cache_entry *ce, struct stat *st)
{
	int match = -1;
	int fd = open(ce->name, O_RDONLY);

	if (fd >= 0) {
		unsigned char sha1[20];
		if (!index_fd(sha1, fd, st, OBJ_BLOB, ce->name, 0))
			match = hashcmp(sha1, ce->sha1);
		/* index_fd() closed the file descriptor already */
	}
	return match;
}

static int ce_compare_link(struct cache_entry *ce, size_t expected_size)
{
	int match = -1;
	void *buffer;
	unsigned long size;
	enum object_type type;
	struct strbuf sb = STRBUF_INIT;

	if (strbuf_readlink(&sb, ce->name, expected_size))
		return -1;

	buffer = read_sha1_file(ce->sha1, &type, &size);
	if (buffer) {
		if (size == sb.len)
			match = memcmp(buffer, sb.buf, size);
		free(buffer);
	}
	strbuf_release(&sb);
	return match;
}

static int ce_compare_gitlink(struct cache_entry *ce)
{
	unsigned char sha1[20];

	/*
	 * We don't actually require that the .git directory
	 * under GITLINK directory be a valid git directory. It
	 * might even be missing (in case nobody populated that
	 * sub-project).
	 *
	 * If so, we consider it always to match.
	 */
	if (resolve_gitlink_ref(ce->name, "HEAD", sha1) < 0)
		return 0;
	return hashcmp(sha1, ce->sha1);
}

int ce_modified_check_fs(struct cache_entry *ce, struct stat *st)
{
	switch (st->st_mode & S_IFMT) {
	case S_IFREG:
		if (ce_compare_data(ce, st))
			return DATA_CHANGED;
		break;
	case S_IFLNK:
		if (ce_compare_link(ce, xsize_t(st->st_size)))
			return DATA_CHANGED;
		break;
	case S_IFDIR:
		if (S_ISGITLINK(ce->ce_mode))
			return ce_compare_gitlink(ce) ? DATA_CHANGED : 0;
	default:
		return TYPE_CHANGED;
	}
	return 0;
}

void set_istate_ops(struct index_state *istate)
{
	if (!istate->version)
		istate->version = INDEX_FORMAT_DEFAULT;

	if (istate->version >= 2 && istate->version <=4)
		istate->ops = &v2_ops;
	if (istate->version == 5)
		istate->ops = &v5_ops;
}

int ce_match_stat_basic(struct index_state *istate,
		struct cache_entry *ce, struct stat *st)
{
	unsigned int changed = 0;

	if (ce->ce_flags & CE_REMOVE)
		return MODE_CHANGED | DATA_CHANGED | TYPE_CHANGED;

	switch (ce->ce_mode & S_IFMT) {
	case S_IFREG:
		changed |= !S_ISREG(st->st_mode) ? TYPE_CHANGED : 0;
		/* We consider only the owner x bit to be relevant for
		 * "mode changes"
		 */
		if (trust_executable_bit &&
		    (0100 & (ce->ce_mode ^ st->st_mode)))
			changed |= MODE_CHANGED;
		break;
	case S_IFLNK:
		if (!S_ISLNK(st->st_mode) &&
		    (has_symlinks || !S_ISREG(st->st_mode)))
			changed |= TYPE_CHANGED;
		break;
	case S_IFGITLINK:
		/* We ignore most of the st_xxx fields for gitlinks */
		if (!S_ISDIR(st->st_mode))
			changed |= TYPE_CHANGED;
		else if (ce_compare_gitlink(ce))
			changed |= DATA_CHANGED;
		return changed;
	default:
		die("internal error: ce_mode is %o", ce->ce_mode);
	}

	set_istate_ops(istate);
	changed = istate->ops->match_stat_basic(ce, st, changed);
	return changed;
}

int is_racy_timestamp(const struct index_state *istate, struct cache_entry *ce)
{
	return (!S_ISGITLINK(ce->ce_mode) &&
		istate->timestamp.sec &&
#ifdef USE_NSEC
		 /* nanosecond timestamped files can also be racy! */
		(istate->timestamp.sec < ce->ce_mtime.sec ||
		 (istate->timestamp.sec == ce->ce_mtime.sec &&
		  istate->timestamp.nsec <= ce->ce_mtime.nsec))
#else
		istate->timestamp.sec <= ce->ce_mtime.sec
#endif
		 );
}

int ie_match_stat(struct index_state *istate,
		  struct cache_entry *ce, struct stat *st,
		  unsigned int options)
{
	unsigned int changed;
	int ignore_valid = options & CE_MATCH_IGNORE_VALID;
	int ignore_skip_worktree = options & CE_MATCH_IGNORE_SKIP_WORKTREE;
	int assume_racy_is_modified = options & CE_MATCH_RACY_IS_DIRTY;

	/*
	 * If it's marked as always valid in the index, it's
	 * valid whatever the checked-out copy says.
	 *
	 * skip-worktree has the same effect with higher precedence
	 */
	if (!ignore_skip_worktree && ce_skip_worktree(ce))
		return 0;
	if (!ignore_valid && (ce->ce_flags & CE_VALID))
		return 0;

	/*
	 * Intent-to-add entries have not been added, so the index entry
	 * by definition never matches what is in the work tree until it
	 * actually gets added.
	 */
	if (ce->ce_flags & CE_INTENT_TO_ADD)
		return DATA_CHANGED | TYPE_CHANGED | MODE_CHANGED;

	changed = ce_match_stat_basic(istate, ce, st);

	/*
	 * Within 1 second of this sequence:
	 * 	echo xyzzy >file && git-update-index --add file
	 * running this command:
	 * 	echo frotz >file
	 * would give a falsely clean cache entry.  The mtime and
	 * length match the cache, and other stat fields do not change.
	 *
	 * We could detect this at update-index time (the cache entry
	 * being registered/updated records the same time as "now")
	 * and delay the return from git-update-index, but that would
	 * effectively mean we can make at most one commit per second,
	 * which is not acceptable.  Instead, we check cache entries
	 * whose mtime are the same as the index file timestamp more
	 * carefully than others.
	 */
	if (!changed && is_racy_timestamp(istate, ce)) {
		if (assume_racy_is_modified)
			changed |= DATA_CHANGED;
		else
			changed |= ce_modified_check_fs(ce, st);
	}

	return changed;
}

int ie_modified(struct index_state *istate,
		struct cache_entry *ce, struct stat *st, unsigned int options)
{
	int changed, changed_fs;

	changed = ie_match_stat(istate, ce, st, options);
	if (!changed)
		return 0;
	/*
	 * If the mode or type has changed, there's no point in trying
	 * to refresh the entry - it's not going to match
	 */
	if (changed & (MODE_CHANGED | TYPE_CHANGED))
		return changed;

	/*
	 * Immediately after read-tree or update-index --cacheinfo,
	 * the length field is zero, as we have never even read the
	 * lstat(2) information once, and we cannot trust DATA_CHANGED
	 * returned by ie_match_stat() which in turn was returned by
	 * ce_match_stat_basic() to signal that the filesize of the
	 * blob changed.  We have to actually go to the filesystem to
	 * see if the contents match, and if so, should answer "unchanged".
	 *
	 * The logic does not apply to gitlinks, as ce_match_stat_basic()
	 * already has checked the actual HEAD from the filesystem in the
	 * subproject.  If ie_match_stat() already said it is different,
	 * then we know it is.
	 */
	if ((changed & DATA_CHANGED) &&
	    (S_ISGITLINK(ce->ce_mode) || ce->ce_size != 0))
		return changed;

	changed_fs = ce_modified_check_fs(ce, st);
	if (changed_fs)
		return changed | changed_fs;
	return 0;
}

int base_name_compare(const char *name1, int len1, int mode1,
		      const char *name2, int len2, int mode2)
{
	unsigned char c1, c2;
	int len = len1 < len2 ? len1 : len2;
	int cmp;

	cmp = memcmp(name1, name2, len);
	if (cmp)
		return cmp;
	c1 = name1[len];
	c2 = name2[len];
	if (!c1 && S_ISDIR(mode1))
		c1 = '/';
	if (!c2 && S_ISDIR(mode2))
		c2 = '/';
	return (c1 < c2) ? -1 : (c1 > c2) ? 1 : 0;
}

/*
 * df_name_compare() is identical to base_name_compare(), except it
 * compares conflicting directory/file entries as equal. Note that
 * while a directory name compares as equal to a regular file, they
 * then individually compare _differently_ to a filename that has
 * a dot after the basename (because '\0' < '.' < '/').
 *
 * This is used by routines that want to traverse the git namespace
 * but then handle conflicting entries together when possible.
 */
int df_name_compare(const char *name1, int len1, int mode1,
		    const char *name2, int len2, int mode2)
{
	int len = len1 < len2 ? len1 : len2, cmp;
	unsigned char c1, c2;

	cmp = memcmp(name1, name2, len);
	if (cmp)
		return cmp;
	/* Directories and files compare equal (same length, same name) */
	if (len1 == len2)
		return 0;
	c1 = name1[len];
	if (!c1 && S_ISDIR(mode1))
		c1 = '/';
	c2 = name2[len];
	if (!c2 && S_ISDIR(mode2))
		c2 = '/';
	if (c1 == '/' && !c2)
		return 0;
	if (c2 == '/' && !c1)
		return 0;
	return c1 - c2;
}

int cache_name_stage_compare(const char *name1, int len1, int stage1, const char *name2, int len2, int stage2)
{
	int len = len1 < len2 ? len1 : len2;
	int cmp;

	cmp = memcmp(name1, name2, len);
	if (cmp)
		return cmp;
	if (len1 < len2)
		return -1;
	if (len1 > len2)
		return 1;

	if (stage1 < stage2)
		return -1;
	if (stage1 > stage2)
		return 1;
	return 0;
}

int cache_name_compare(const char *name1, int len1, const char *name2, int len2)
{
	return cache_name_stage_compare(name1, len1, 0, name2, len2, 0);
}

int index_name_stage_pos(const struct index_state *istate, const char *name, int namelen, int stage)
{
	int first, last;

	first = 0;
	last = istate->cache_nr;
	while (last > first) {
		int next = (last + first) >> 1;
		struct cache_entry *ce = istate->cache[next];
		int cmp = cache_name_stage_compare(name, namelen, stage, ce->name, ce_namelen(ce), ce_stage(ce));
		if (!cmp)
			return next;
		if (cmp < 0) {
			last = next;
			continue;
		}
		first = next+1;
	}
	return -first-1;
}

int index_name_pos(const struct index_state *istate, const char *name, int namelen)
{
	return index_name_stage_pos(istate, name, namelen, 0);
}

/* Remove entry, return true if there are more entries to go.. */
int remove_index_entry_at(struct index_state *istate, int pos)
{
	struct cache_entry *ce = istate->cache[pos];

	record_resolve_undo(istate, ce);
	remove_name_hash(ce);
	istate->cache_changed = 1;
	istate->cache_nr--;
	if (pos >= istate->cache_nr)
		return 0;
	memmove(istate->cache + pos,
		istate->cache + pos + 1,
		(istate->cache_nr - pos) * sizeof(struct cache_entry *));
	return 1;
}

/*
 * Remove all cache ententries marked for removal, that is where
 * CE_REMOVE is set in ce_flags.  This is much more effective than
 * calling remove_index_entry_at() for each entry to be removed.
 */
void remove_marked_cache_entries(struct index_state *istate)
{
	struct cache_entry **ce_array = istate->cache;
	unsigned int i, j;

	for (i = j = 0; i < istate->cache_nr; i++) {
		if (ce_array[i]->ce_flags & CE_REMOVE)
			remove_name_hash(ce_array[i]);
		else
			ce_array[j++] = ce_array[i];
	}
	istate->cache_changed = 1;
	istate->cache_nr = j;
}

int remove_file_from_index(struct index_state *istate, const char *path)
{
	int pos = index_name_pos(istate, path, strlen(path));
	if (pos < 0)
		pos = -pos-1;
	cache_tree_invalidate_path(istate->cache_tree, path);
	while (pos < istate->cache_nr && !strcmp(istate->cache[pos]->name, path))
		remove_index_entry_at(istate, pos);
	return 0;
}

static int compare_name(struct cache_entry *ce, const char *path, int namelen)
{
	return namelen != ce_namelen(ce) || memcmp(path, ce->name, namelen);
}

static int index_name_pos_also_unmerged(struct index_state *istate,
	const char *path, int namelen)
{
	int pos = index_name_pos(istate, path, namelen);
	struct cache_entry *ce;

	if (pos >= 0)
		return pos;

	/* maybe unmerged? */
	pos = -1 - pos;
	if (pos >= istate->cache_nr ||
			compare_name((ce = istate->cache[pos]), path, namelen))
		return -1;

	/* order of preference: stage 2, 1, 3 */
	if (ce_stage(ce) == 1 && pos + 1 < istate->cache_nr &&
			ce_stage((ce = istate->cache[pos + 1])) == 2 &&
			!compare_name(ce, path, namelen))
		pos++;
	return pos;
}

static int different_name(struct cache_entry *ce, struct cache_entry *alias)
{
	int len = ce_namelen(ce);
	return ce_namelen(alias) != len || memcmp(ce->name, alias->name, len);
}

/*
 * If we add a filename that aliases in the cache, we will use the
 * name that we already have - but we don't want to update the same
 * alias twice, because that implies that there were actually two
 * different files with aliasing names!
 *
 * So we use the CE_ADDED flag to verify that the alias was an old
 * one before we accept it as
 */
static struct cache_entry *create_alias_ce(struct cache_entry *ce, struct cache_entry *alias)
{
	int len;
	struct cache_entry *new;

	if (alias->ce_flags & CE_ADDED)
		die("Will not add file alias '%s' ('%s' already exists in index)", ce->name, alias->name);

	/* Ok, create the new entry using the name of the existing alias */
	len = ce_namelen(alias);
	new = xcalloc(1, cache_entry_size(len));
	memcpy(new->name, alias->name, len);
	copy_cache_entry(new, ce);
	free(ce);
	return new;
}

static void record_intent_to_add(struct cache_entry *ce)
{
	unsigned char sha1[20];
	if (write_sha1_file("", 0, blob_type, sha1))
		die("cannot create an empty blob in the object database");
	hashcpy(ce->sha1, sha1);
}

int add_to_index(struct index_state *istate, const char *path, struct stat *st, int flags)
{
	int size, namelen, was_same;
	mode_t st_mode = st->st_mode;
	struct cache_entry *ce, *alias;
	unsigned ce_option = CE_MATCH_IGNORE_VALID|CE_MATCH_IGNORE_SKIP_WORKTREE|CE_MATCH_RACY_IS_DIRTY;
	int verbose = flags & (ADD_CACHE_VERBOSE | ADD_CACHE_PRETEND);
	int pretend = flags & ADD_CACHE_PRETEND;
	int intent_only = flags & ADD_CACHE_INTENT;
	int add_option = (ADD_CACHE_OK_TO_ADD|ADD_CACHE_OK_TO_REPLACE|
			  (intent_only ? ADD_CACHE_NEW_ONLY : 0));

	if (!S_ISREG(st_mode) && !S_ISLNK(st_mode) && !S_ISDIR(st_mode))
		return error("%s: can only add regular files, symbolic links or git-directories", path);

	namelen = strlen(path);
	if (S_ISDIR(st_mode)) {
		while (namelen && path[namelen-1] == '/')
			namelen--;
	}
	size = cache_entry_size(namelen);
	ce = xcalloc(1, size);
	memcpy(ce->name, path, namelen);
	ce->ce_namelen = namelen;
	if (!intent_only)
		fill_stat_cache_info(ce, st);
	else
		ce->ce_flags |= CE_INTENT_TO_ADD;

	if (trust_executable_bit && has_symlinks)
		ce->ce_mode = create_ce_mode(st_mode);
	else {
		/* If there is an existing entry, pick the mode bits and type
		 * from it, otherwise assume unexecutable regular file.
		 */
		struct cache_entry *ent;
		int pos = index_name_pos_also_unmerged(istate, path, namelen);

		ent = (0 <= pos) ? istate->cache[pos] : NULL;
		ce->ce_mode = ce_mode_from_stat(ent, st_mode);
	}

	/* When core.ignorecase=true, determine if a directory of the same name but differing
	 * case already exists within the Git repository.  If it does, ensure the directory
	 * case of the file being added to the repository matches (is folded into) the existing
	 * entry's directory case.
	 */
	if (ignore_case) {
		const char *startPtr = ce->name;
		const char *ptr = startPtr;
		while (*ptr) {
			while (*ptr && *ptr != '/')
				++ptr;
			if (*ptr == '/') {
				struct cache_entry *foundce;
				++ptr;
				foundce = index_name_exists(&the_index, ce->name, ptr - ce->name, ignore_case);
				if (foundce) {
					memcpy((void *)startPtr, foundce->name + (startPtr - ce->name), ptr - startPtr);
					startPtr = ptr;
				}
			}
		}
	}

	alias = index_name_exists(istate, ce->name, ce_namelen(ce), ignore_case);
	if (alias && !ce_stage(alias) && !ie_match_stat(istate, alias, st, ce_option)) {
		/* Nothing changed, really */
		free(ce);
		if (!S_ISGITLINK(alias->ce_mode))
			ce_mark_uptodate(alias);
		alias->ce_flags |= CE_ADDED;
		return 0;
	}
	if (!intent_only) {
		if (index_path(ce->sha1, path, st, HASH_WRITE_OBJECT))
			return error("unable to index file %s", path);
	} else
		record_intent_to_add(ce);

	if (ignore_case && alias && different_name(ce, alias))
		ce = create_alias_ce(ce, alias);
	ce->ce_flags |= CE_ADDED;

	/* It was suspected to be racily clean, but it turns out to be Ok */
	was_same = (alias &&
		    !ce_stage(alias) &&
		    !hashcmp(alias->sha1, ce->sha1) &&
		    ce->ce_mode == alias->ce_mode);

	if (pretend)
		;
	else if (add_index_entry(istate, ce, add_option))
		return error("unable to add %s to index",path);
	if (verbose && !was_same)
		printf("add '%s'\n", path);
	return 0;
}

int add_file_to_index(struct index_state *istate, const char *path, int flags)
{
	struct stat st;
	if (lstat(path, &st))
		die_errno("unable to stat '%s'", path);
	return add_to_index(istate, path, &st, flags);
}

struct cache_entry *make_cache_entry(unsigned int mode,
		const unsigned char *sha1, const char *path, int stage,
		int refresh)
{
	int size, len;
	struct cache_entry *ce;

	if (!verify_path(path)) {
		error("Invalid path '%s'", path);
		return NULL;
	}

	len = strlen(path);
	size = cache_entry_size(len);
	ce = xcalloc(1, size);

	hashcpy(ce->sha1, sha1);
	memcpy(ce->name, path, len);
	ce->ce_flags = create_ce_flags(stage);
	ce->ce_namelen = len;
	ce->ce_mode = create_ce_mode(mode);

	if (refresh)
		return refresh_cache_entry(ce, 0);

	return ce;
}

int ce_same_name(struct cache_entry *a, struct cache_entry *b)
{
	int len = ce_namelen(a);
	return ce_namelen(b) == len && !memcmp(a->name, b->name, len);
}

int ce_path_match(const struct cache_entry *ce, const struct pathspec *pathspec)
{
	return match_pathspec_depth(pathspec, ce->name, ce_namelen(ce), 0, NULL);
}

/*
 * We fundamentally don't like some paths: we don't want
 * dot or dot-dot anywhere, and for obvious reasons don't
 * want to recurse into ".git" either.
 *
 * Also, we don't want double slashes or slashes at the
 * end that can make pathnames ambiguous.
 */
static int verify_dotfile(const char *rest)
{
	/*
	 * The first character was '.', but that
	 * has already been discarded, we now test
	 * the rest.
	 */

	/* "." is not allowed */
	if (*rest == '\0' || is_dir_sep(*rest))
		return 0;

	switch (*rest) {
	/*
	 * ".git" followed by  NUL or slash is bad. This
	 * shares the path end test with the ".." case.
	 */
	case 'g':
		if (rest[1] != 'i')
			break;
		if (rest[2] != 't')
			break;
		rest += 2;
	/* fallthrough */
	case '.':
		if (rest[1] == '\0' || is_dir_sep(rest[1]))
			return 0;
	}
	return 1;
}

int verify_path(const char *path)
{
	char c;

	if (has_dos_drive_prefix(path))
		return 0;

	goto inside;
	for (;;) {
		if (!c)
			return 1;
		if (is_dir_sep(c)) {
inside:
			c = *path++;
			if ((c == '.' && !verify_dotfile(path)) ||
			    is_dir_sep(c) || c == '\0')
				return 0;
		}
		c = *path++;
	}
}

/*
 * Do we have another file that has the beginning components being a
 * proper superset of the name we're trying to add?
 */
static int has_file_name(struct index_state *istate,
			 const struct cache_entry *ce, int pos, int ok_to_replace)
{
	int retval = 0;
	int len = ce_namelen(ce);
	int stage = ce_stage(ce);
	const char *name = ce->name;

	while (pos < istate->cache_nr) {
		struct cache_entry *p = istate->cache[pos++];

		if (len >= ce_namelen(p))
			break;
		if (memcmp(name, p->name, len))
			break;
		if (ce_stage(p) != stage)
			continue;
		if (p->name[len] != '/')
			continue;
		if (p->ce_flags & CE_REMOVE)
			continue;
		retval = -1;
		if (!ok_to_replace)
			break;
		remove_index_entry_at(istate, --pos);
	}
	return retval;
}

/*
 * Do we have another file with a pathname that is a proper
 * subset of the name we're trying to add?
 */
static int has_dir_name(struct index_state *istate,
			const struct cache_entry *ce, int pos, int ok_to_replace)
{
	int retval = 0;
	int stage = ce_stage(ce);
	const char *name = ce->name;
	const char *slash = name + ce_namelen(ce);

	for (;;) {
		int len;

		for (;;) {
			if (*--slash == '/')
				break;
			if (slash <= ce->name)
				return retval;
		}
		len = slash - name;

		pos = index_name_stage_pos(istate, name, len, stage);
		if (pos >= 0) {
			/*
			 * Found one, but not so fast.  This could
			 * be a marker that says "I was here, but
			 * I am being removed".  Such an entry is
			 * not a part of the resulting tree, and
			 * it is Ok to have a directory at the same
			 * path.
			 */
			if (!(istate->cache[pos]->ce_flags & CE_REMOVE)) {
				retval = -1;
				if (!ok_to_replace)
					break;
				remove_index_entry_at(istate, pos);
				continue;
			}
		}
		else
			pos = -pos-1;

		/*
		 * Trivial optimization: if we find an entry that
		 * already matches the sub-directory, then we know
		 * we're ok, and we can exit.
		 */
		while (pos < istate->cache_nr) {
			struct cache_entry *p = istate->cache[pos];
			if ((ce_namelen(p) <= len) ||
			    (p->name[len] != '/') ||
			    memcmp(p->name, name, len))
				break; /* not our subdirectory */
			if (ce_stage(p) == stage && !(p->ce_flags & CE_REMOVE))
				/*
				 * p is at the same stage as our entry, and
				 * is a subdirectory of what we are looking
				 * at, so we cannot have conflicts at our
				 * level or anything shorter.
				 */
				return retval;
			pos++;
		}
	}
	return retval;
}

/* We may be in a situation where we already have path/file and path
 * is being added, or we already have path and path/file is being
 * added.  Either one would result in a nonsense tree that has path
 * twice when git-write-tree tries to write it out.  Prevent it.
 *
 * If ok-to-replace is specified, we remove the conflicting entries
 * from the cache so the caller should recompute the insert position.
 * When this happens, we return non-zero.
 */
static int check_file_directory_conflict(struct index_state *istate,
					 const struct cache_entry *ce,
					 int pos, int ok_to_replace)
{
	int retval;

	/*
	 * When ce is an "I am going away" entry, we allow it to be added
	 */
	if (ce->ce_flags & CE_REMOVE)
		return 0;

	/*
	 * We check if the path is a sub-path of a subsequent pathname
	 * first, since removing those will not change the position
	 * in the array.
	 */
	retval = has_file_name(istate, ce, pos, ok_to_replace);

	/*
	 * Then check if the path might have a clashing sub-directory
	 * before it.
	 */
	return retval + has_dir_name(istate, ce, pos, ok_to_replace);
}

static int add_index_entry_with_check(struct index_state *istate, struct cache_entry *ce, int option)
{
	int pos;
	int ok_to_add = option & ADD_CACHE_OK_TO_ADD;
	int ok_to_replace = option & ADD_CACHE_OK_TO_REPLACE;
	int skip_df_check = option & ADD_CACHE_SKIP_DFCHECK;
	int new_only = option & ADD_CACHE_NEW_ONLY;

	cache_tree_invalidate_path(istate->cache_tree, ce->name);
	pos = index_name_stage_pos(istate, ce->name, ce_namelen(ce), ce_stage(ce));

	/* existing match? Just replace it. */
	if (pos >= 0) {
		if (!new_only)
			replace_index_entry(istate, pos, ce);
		return 0;
	}
	pos = -pos-1;

	/*
	 * Inserting a merged entry ("stage 0") into the index
	 * will always replace all non-merged entries..
	 */
	if (pos < istate->cache_nr && ce_stage(ce) == 0) {
		while (ce_same_name(istate->cache[pos], ce)) {
			ok_to_add = 1;
			if (!remove_index_entry_at(istate, pos))
				break;
		}
	}

	if (!ok_to_add)
		return -1;
	if (!verify_path(ce->name))
		return error("Invalid path '%s'", ce->name);

	if (!skip_df_check &&
	    check_file_directory_conflict(istate, ce, pos, ok_to_replace)) {
		if (!ok_to_replace)
			return error("'%s' appears as both a file and as a directory",
				     ce->name);
		pos = index_name_stage_pos(istate, ce->name, ce_namelen(ce), ce_stage(ce));
		pos = -pos-1;
	}
	return pos + 1;
}

int add_index_entry(struct index_state *istate, struct cache_entry *ce, int option)
{
	int pos;

	if (option & ADD_CACHE_JUST_APPEND)
		pos = istate->cache_nr;
	else {
		int ret;
		ret = add_index_entry_with_check(istate, ce, option);
		if (ret <= 0)
			return ret;
		pos = ret - 1;
	}

	/* Make sure the array is big enough .. */
	if (istate->cache_nr == istate->cache_alloc) {
		istate->cache_alloc = alloc_nr(istate->cache_alloc);
		istate->cache = xrealloc(istate->cache,
					istate->cache_alloc * sizeof(struct cache_entry *));
	}

	/* Add it in.. */
	istate->cache_nr++;
	if (istate->cache_nr > pos + 1)
		memmove(istate->cache + pos + 1,
			istate->cache + pos,
			(istate->cache_nr - pos - 1) * sizeof(ce));
	set_index_entry(istate, pos, ce);
	istate->cache_changed = 1;
	return 0;
}

/*
 * "refresh" does not calculate a new sha1 file or bring the
 * cache up-to-date for mode/content changes. But what it
 * _does_ do is to "re-match" the stat information of a file
 * with the cache, so that you can refresh the cache for a
 * file that hasn't been changed but where the stat entry is
 * out of date.
 *
 * For example, you'd want to do this after doing a "git-read-tree",
 * to link up the stat cache details with the proper files.
 */
static struct cache_entry *refresh_cache_ent(struct index_state *istate,
					     struct cache_entry *ce,
					     unsigned int options, int *err,
					     int *changed_ret)
{
	struct stat st;
	struct cache_entry *updated;
	int changed, size;
	int ignore_valid = options & CE_MATCH_IGNORE_VALID;
	int ignore_skip_worktree = options & CE_MATCH_IGNORE_SKIP_WORKTREE;

	if (ce_uptodate(ce))
		return ce;

	/*
	 * CE_VALID or CE_SKIP_WORKTREE means the user promised us
	 * that the change to the work tree does not matter and told
	 * us not to worry.
	 */
	if (!ignore_skip_worktree && ce_skip_worktree(ce)) {
		ce_mark_uptodate(ce);
		return ce;
	}
	if (!ignore_valid && (ce->ce_flags & CE_VALID)) {
		ce_mark_uptodate(ce);
		return ce;
	}

	if (lstat(ce->name, &st) < 0) {
		if (err)
			*err = errno;
		return NULL;
	}

	changed = ie_match_stat(istate, ce, &st, options);
	if (changed_ret)
		*changed_ret = changed;
	if (!changed) {
		/*
		 * The path is unchanged.  If we were told to ignore
		 * valid bit, then we did the actual stat check and
		 * found that the entry is unmodified.  If the entry
		 * is not marked VALID, this is the place to mark it
		 * valid again, under "assume unchanged" mode.
		 */
		if (ignore_valid && assume_unchanged &&
		    !(ce->ce_flags & CE_VALID))
			; /* mark this one VALID again */
		else {
			/*
			 * We do not mark the index itself "modified"
			 * because CE_UPTODATE flag is in-core only;
			 * we are not going to write this change out.
			 */
			if (!S_ISGITLINK(ce->ce_mode))
				ce_mark_uptodate(ce);
			return ce;
		}
	}

	if (ie_modified(istate, ce, &st, options)) {
		if (err)
			*err = EINVAL;
		return NULL;
	}

	size = ce_size(ce);
	updated = xmalloc(size);
	memcpy(updated, ce, size);
	fill_stat_cache_info(updated, &st);
	/*
	 * If ignore_valid is not set, we should leave CE_VALID bit
	 * alone.  Otherwise, paths marked with --no-assume-unchanged
	 * (i.e. things to be edited) will reacquire CE_VALID bit
	 * automatically, which is not really what we want.
	 */
	if (!ignore_valid && assume_unchanged &&
	    !(ce->ce_flags & CE_VALID))
		updated->ce_flags &= ~CE_VALID;

	return updated;
}

static void show_file(const char * fmt, const char * name, int in_porcelain,
		      int * first, const char *header_msg)
{
	if (in_porcelain && *first && header_msg) {
		printf("%s\n", header_msg);
		*first = 0;
	}
	printf(fmt, name);
}

int refresh_index(struct index_state *istate, unsigned int flags, const char **pathspec,
		  char *seen, const char *header_msg)
{
	int i;
	int has_errors = 0;
	int really = (flags & REFRESH_REALLY) != 0;
	int allow_unmerged = (flags & REFRESH_UNMERGED) != 0;
	int quiet = (flags & REFRESH_QUIET) != 0;
	int not_new = (flags & REFRESH_IGNORE_MISSING) != 0;
	int ignore_submodules = (flags & REFRESH_IGNORE_SUBMODULES) != 0;
	int first = 1;
	int in_porcelain = (flags & REFRESH_IN_PORCELAIN);
	unsigned int options = really ? CE_MATCH_IGNORE_VALID : 0;
	const char *modified_fmt;
	const char *deleted_fmt;
	const char *typechange_fmt;
	const char *added_fmt;
	const char *unmerged_fmt;

	modified_fmt = (in_porcelain ? "M\t%s\n" : "%s: needs update\n");
	deleted_fmt = (in_porcelain ? "D\t%s\n" : "%s: needs update\n");
	typechange_fmt = (in_porcelain ? "T\t%s\n" : "%s needs update\n");
	added_fmt = (in_porcelain ? "A\t%s\n" : "%s needs update\n");
	unmerged_fmt = (in_porcelain ? "U\t%s\n" : "%s: needs merge\n");
	for (i = 0; i < istate->cache_nr; i++) {
		struct cache_entry *ce, *new;
		int cache_errno = 0;
		int changed = 0;
		int filtered = 0;

		ce = istate->cache[i];
		if (ignore_submodules && S_ISGITLINK(ce->ce_mode))
			continue;

		if (pathspec &&
		    !match_pathspec(pathspec, ce->name, ce_namelen(ce), 0, seen))
			filtered = 1;

		if (ce_stage(ce)) {
			while ((i < istate->cache_nr) &&
			       ! strcmp(istate->cache[i]->name, ce->name))
				i++;
			i--;
			if (allow_unmerged)
				continue;
			if (!filtered)
				show_file(unmerged_fmt, ce->name, in_porcelain,
					  &first, header_msg);
			has_errors = 1;
			continue;
		}

		if (filtered)
			continue;

		new = refresh_cache_ent(istate, ce, options, &cache_errno, &changed);
		if (new == ce)
			continue;
		if (!new) {
			const char *fmt;

			if (not_new && cache_errno == ENOENT)
				continue;
			if (really && cache_errno == EINVAL) {
				/* If we are doing --really-refresh that
				 * means the index is not valid anymore.
				 */
				ce->ce_flags &= ~CE_VALID;
				istate->cache_changed = 1;
			}
			if (quiet)
				continue;

			if (cache_errno == ENOENT)
				fmt = deleted_fmt;
			else if (ce->ce_flags & CE_INTENT_TO_ADD)
				fmt = added_fmt; /* must be before other checks */
			else if (changed & TYPE_CHANGED)
				fmt = typechange_fmt;
			else
				fmt = modified_fmt;
			show_file(fmt,
				  ce->name, in_porcelain, &first, header_msg);
			has_errors = 1;
			continue;
		}

		replace_index_entry(istate, i, new);
	}
	return has_errors;
}

static struct cache_entry *refresh_cache_entry(struct cache_entry *ce, int really)
{
	return refresh_cache_ent(&the_index, ce, really, NULL, NULL);
}

static int verify_hdr_version(struct index_state *istate,
		struct cache_version_header *hdr, unsigned long size)
{
	int hdr_version;

	if (hdr->hdr_signature != htonl(CACHE_SIGNATURE))
		return error("bad signature");
	hdr_version = ntohl(hdr->hdr_version);
	if (hdr_version >= 2 && hdr_version <= 4)
		istate->ops = &v2_ops;
	else if (hdr_version == 5)
		istate->ops = &v5_ops;
	else
		return error("bad index version %d", hdr_version);
	return 0;
}

int read_index(struct index_state *istate)
{
	return read_index_from(istate, get_index_file());
}

static int index_changed(struct stat st_old, struct stat st_new)
{
	int changed = 0;

	if (st_old.st_mtime != st_new.st_mtime ||
	    st_old.st_uid   != st_new.st_uid ||
	    st_old.st_gid   != st_new.st_gid ||
	    st_old.st_ino   != st_new.st_ino ||
	    st_old.st_size  != st_new.st_size)
		changed = 1;
#ifdef USE_NSEC
	if (ST_MTIME_NSEC(st_old) != ST_MTIME_NSEC(st_new))
		changed = 1;
#endif

#ifdef USE_STDEV
	if (st_old.st_dev != st_new.st_dev)
		changed = 1;
#endif

	return changed;
}

/* remember to discard_cache() before reading a different cache! */
int read_index_from(struct index_state *istate, const char *path)
{
	int fd, err, i = 0;
	struct stat st_old, st_new;
	struct cache_version_header *hdr;
	void *mmap;
	size_t mmap_size;

	if (istate->initialized)
		return istate->cache_nr;

	istate->timestamp.sec = 0;
	istate->timestamp.nsec = 0;
	do {
		err = 0;
		fd = open(path, O_RDONLY);
		if (fd < 0) {
			if (errno == ENOENT)
				return 0;
			die_errno("index file open failed");
		}

		if (fstat(fd, &st_old))
			die_errno("cannot stat the open index");

		errno = EINVAL;
		mmap_size = xsize_t(st_old.st_size);
		mmap = xmmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
		close(fd);
		if (mmap == MAP_FAILED)
			die_errno("unable to map index file");

		hdr = mmap;
		if (verify_hdr_version(istate, hdr, mmap_size) < 0)
			err = 1;

		if (istate->ops->verify_hdr(mmap, mmap_size) < 0)
			err = 1;

		if (istate->ops->read_index(istate, mmap, mmap_size) < 0)
			err = 1;
		istate->timestamp.sec = st_old.st_mtime;
		istate->timestamp.nsec = ST_MTIME_NSEC(st_old);
		if (lstat(path, &st_new))
			die_errno("cannot stat the open index");

		munmap(mmap, mmap_size);

		if (!index_changed(st_old, st_new) && !err)
			return istate->cache_nr;

		usleep(10*1000);
		i++;
	} while ((err || index_changed(st_old, st_new)) && i < 50);

	munmap(mmap, mmap_size);
	die("index file corrupt");
}

int is_index_unborn(struct index_state *istate)
{
	return (!istate->cache_nr && !istate->timestamp.sec);
}

int discard_index(struct index_state *istate)
{
	int i;

	for (i = 0; i < istate->cache_nr; i++)
		free(istate->cache[i]);
	resolve_undo_clear_index(istate);
	istate->cache_nr = 0;
	istate->cache_changed = 0;
	istate->timestamp.sec = 0;
	istate->timestamp.nsec = 0;
	istate->name_hash_initialized = 0;
	free_hash(&istate->name_hash);
	cache_tree_free(&(istate->cache_tree));
	istate->initialized = 0;

	/* no need to throw away allocated active_cache */
	return 0;
}

int unmerged_index(const struct index_state *istate)
{
	int i;
	for (i = 0; i < istate->cache_nr; i++) {
		if (ce_stage(istate->cache[i]))
			return 1;
	}
	return 0;
}

static int has_racy_timestamp(struct index_state *istate)
{
	int entries = istate->cache_nr;
	int i;

	for (i = 0; i < entries; i++) {
		struct cache_entry *ce = istate->cache[i];
		if (is_racy_timestamp(istate, ce))
			return 1;
	}
	return 0;
}

/*
 * Opportunisticly update the index but do not complain if we can't
 */
void update_index_if_able(struct index_state *istate, struct lock_file *lockfile)
{
	if ((istate->cache_changed || has_racy_timestamp(istate)) &&
	    !write_index(istate, lockfile->fd))
		commit_locked_index(lockfile);
	else
		rollback_lock_file(lockfile);
}
int write_index(struct index_state *istate, int newfd)
{
	set_istate_ops(istate);

	return istate->ops->write_index(istate, newfd);
}

/*
 * Read the index file that is potentially unmerged into given
 * index_state, dropping any unmerged entries.  Returns true if
 * the index is unmerged.  Callers who want to refuse to work
 * from an unmerged state can call this and check its return value,
 * instead of calling read_cache().
 */
int read_index_unmerged(struct index_state *istate)
{
	int i;
	int unmerged = 0;

	read_index(istate);
	for (i = 0; i < istate->cache_nr; i++) {
		struct cache_entry *ce = istate->cache[i];
		struct cache_entry *new_ce;
		int size, len;

		if (!ce_stage(ce))
			continue;
		unmerged = 1;
		len = ce_namelen(ce);
		size = cache_entry_size(len);
		new_ce = xcalloc(1, size);
		memcpy(new_ce->name, ce->name, len);
		new_ce->ce_flags = create_ce_flags(0) | CE_CONFLICTED;
		new_ce->ce_namelen = len;
		new_ce->ce_mode = ce->ce_mode;
		if (add_index_entry(istate, new_ce, 0))
			return error("%s: cannot drop to stage #0",
				     ce->name);
		i = index_name_pos(istate, new_ce->name, len);
	}
	return unmerged;
}

/*
 * Returns 1 if the path is an "other" path with respect to
 * the index; that is, the path is not mentioned in the index at all,
 * either as a file, a directory with some files in the index,
 * or as an unmerged entry.
 *
 * We helpfully remove a trailing "/" from directories so that
 * the output of read_directory can be used as-is.
 */
int index_name_is_other(const struct index_state *istate, const char *name,
		int namelen)
{
	int pos;
	if (namelen && name[namelen - 1] == '/')
		namelen--;
	pos = index_name_pos(istate, name, namelen);
	if (0 <= pos)
		return 0;	/* exact match */
	pos = -pos - 1;
	if (pos < istate->cache_nr) {
		struct cache_entry *ce = istate->cache[pos];
		if (ce_namelen(ce) == namelen &&
		    !memcmp(ce->name, name, namelen))
			return 0; /* Yup, this one exists unmerged */
	}
	return 1;
}
