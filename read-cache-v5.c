#include "cache.h"
#include "read-cache.h"
#include "string-list.h"
#include "resolve-undo.h"
#include "cache-tree.h"
#include "dir.h"
#include "pathspec.h"

#define ptr_add(x,y) ((void *)(((char *)(x)) + (y)))

struct cache_header_v5 {
	uint32_t hdr_ndir;
	uint32_t hdr_fblockoffset;
	uint32_t hdr_nextension;
};

struct directory_entry {
	struct directory_entry **sub;
	struct directory_entry *next;
	struct directory_entry *next_hash;
	struct cache_entry *ce;
	struct cache_entry *ce_last;
	struct conflict_entry *conflict;
	struct conflict_entry *conflict_last;
	uint32_t conflict_size;
	uint32_t de_foffset;
	uint32_t de_cr;
	uint32_t de_ncr;
	uint32_t de_nsubtrees;
	uint32_t de_nfiles;
	uint32_t de_nentries;
	unsigned char sha1[20];
	uint16_t de_flags;
	uint32_t de_pathlen;
	char pathname[FLEX_ARRAY];
};

struct conflict_part {
	struct conflict_part *next;
	uint16_t flags;
	uint16_t entry_mode;
	unsigned char sha1[20];
};

struct conflict_entry {
	struct conflict_entry *next;
	uint32_t nfileconflicts;
	struct conflict_part *entries;
	uint32_t namelen;
	uint32_t pathlen;
	char name[FLEX_ARRAY];
};

#define directory_entry_size(len) (offsetof(struct directory_entry,pathname) + (len) + 1)
#define conflict_entry_size(len) (offsetof(struct conflict_entry,name) + (len) + 1)

/*****************************************************************
 * Index File I/O
 *****************************************************************/

struct ondisk_conflict_part {
	uint16_t flags;
	uint16_t entry_mode;
	unsigned char sha1[20];
};

struct ondisk_cache_entry {
	uint16_t flags;
	uint16_t mode;
	struct cache_time mtime;
	uint32_t size;
	int stat_crc;
	unsigned char sha1[20];
};

struct ondisk_directory_entry {
	uint32_t foffset;
	uint32_t cr;
	uint32_t ncr;
	uint32_t nsubtrees;
	uint32_t nfiles;
	uint32_t nentries;
	unsigned char sha1[20];
	uint16_t flags;
};

static int check_crc32(int initialcrc,
			void *data,
			size_t len,
			unsigned int expected_crc)
{
	int crc;

	crc = crc32(initialcrc, (Bytef*)data, len);
	return crc == expected_crc;
}

static int match_stat_crc(struct stat *st, uint32_t expected_crc)
{
	uint32_t data, stat_crc = 0;
	unsigned int ctimens = 0;

	data = htonl(st->st_ctime);
	stat_crc = crc32(0, (Bytef*)&data, 4);
#ifdef USE_NSEC
	ctimens = ST_CTIME_NSEC(*st);
#endif
	data = htonl(ctimens);
	stat_crc = crc32(stat_crc, (Bytef*)&data, 4);
	data = htonl(st->st_ino);
	stat_crc = crc32(stat_crc, (Bytef*)&data, 4);
	data = htonl(st->st_dev);
	stat_crc = crc32(stat_crc, (Bytef*)&data, 4);
	data = htonl(st->st_uid);
	stat_crc = crc32(stat_crc, (Bytef*)&data, 4);
	data = htonl(st->st_gid);
	stat_crc = crc32(stat_crc, (Bytef*)&data, 4);

	return stat_crc == expected_crc;
}

static int match_stat_basic(const struct cache_entry *ce,
			    struct stat *st,
			    int changed)
{

	if (ce->ce_stat_data.sd_mtime.sec != (unsigned int)st->st_mtime)
		changed |= MTIME_CHANGED;
#ifdef USE_NSEC
	if (ce->ce_stat_data.sd_mtime.nsec != ST_MTIME_NSEC(*st))
		changed |= MTIME_CHANGED;
#endif
	if (ce->ce_stat_data.sd_size != (unsigned int)st->st_size)
		changed |= DATA_CHANGED;

	if (trust_ctime && ce->ce_stat_crc != 0 && !match_stat_crc(st, ce->ce_stat_crc)) {
		changed |= OWNER_CHANGED;
		changed |= INODE_CHANGED;
	}
	/* Racily smudged entry? */
	if (ce->ce_flags & CE_SMUDGED) {
		if (!changed && !is_empty_blob_sha1(ce->sha1) && ce_modified_check_fs(ce, st))
			changed |= DATA_CHANGED;
	}
	return changed;
}

static int verify_hdr(void *mmap, unsigned long size)
{
	uint32_t *filecrc;
	unsigned int header_size;
	struct cache_header *hdr;
	struct cache_header_v5 *hdr_v5;

	if (size < sizeof(struct cache_header)
	    + sizeof (struct cache_header_v5) + 4)
		die("index file smaller than expected");

	hdr = mmap;
	hdr_v5 = ptr_add(mmap, sizeof(*hdr));
	/* Size of the header + the size of the extensionoffsets */
	header_size = sizeof(*hdr) + sizeof(*hdr_v5) + hdr_v5->hdr_nextension * 4;
	/* Initialize crc */
	filecrc = ptr_add(mmap, header_size);
	if (!check_crc32(0, hdr, header_size, ntohl(*filecrc)))
		return error("bad index file header crc signature");
	return 0;
}

static struct cache_entry *cache_entry_from_ondisk(struct ondisk_cache_entry *ondisk,
						   char *pathname,
						   char *name,
						   size_t len,
						   size_t pathlen)
{
	struct cache_entry *ce = xmalloc(cache_entry_size(len + pathlen));
	int flags;

	flags = ntoh_s(ondisk->flags);
	/*
	 * This entry was invalidated in the index file,
	 * we don't need any data from it
	 */
	if (flags & CE_INVALID_V5)
		return NULL;
	ce->ce_stat_data.sd_ctime.sec  = 0;
	ce->ce_stat_data.sd_mtime.sec  = ntoh_l(ondisk->mtime.sec);
	ce->ce_stat_data.sd_ctime.nsec = 0;
	ce->ce_stat_data.sd_mtime.nsec = ntoh_l(ondisk->mtime.nsec);
	ce->ce_stat_data.sd_dev        = 0;
	ce->ce_stat_data.sd_ino        = 0;
	ce->ce_stat_data.sd_uid        = 0;
	ce->ce_stat_data.sd_gid        = 0;
	ce->ce_stat_data.sd_size       = ntoh_l(ondisk->size);
	ce->ce_mode       = ntoh_s(ondisk->mode);
	ce->ce_flags      = flags & CE_STAGEMASK;
	ce->ce_flags     |= flags & CE_VALID;
	ce->ce_flags     |= flags & CE_SMUDGED;
	if (flags & CE_INTENT_TO_ADD_V5)
		ce->ce_flags |= CE_INTENT_TO_ADD;
	if (flags & CE_SKIP_WORKTREE_V5)
		ce->ce_flags |= CE_SKIP_WORKTREE;
	ce->ce_stat_crc   = ntoh_l(ondisk->stat_crc);
	ce->ce_namelen    = len + pathlen;
	hashcpy(ce->sha1, ondisk->sha1);
	memcpy(ce->name, pathname, pathlen);
	memcpy(ce->name + pathlen, name, len);
	ce->name[len + pathlen] = '\0';
	return ce;
}

static struct directory_entry *directory_entry_from_ondisk(struct ondisk_directory_entry *ondisk,
						   const char *name,
						   size_t len)
{
	struct directory_entry *de = xmalloc(directory_entry_size(len));

	memcpy(de->pathname, name, len);
	de->pathname[len] = '\0';
	de->de_flags      = ntoh_s(ondisk->flags);
	de->de_foffset    = ntoh_l(ondisk->foffset);
	de->de_cr         = ntoh_l(ondisk->cr);
	de->de_ncr        = ntoh_l(ondisk->ncr);
	de->de_nsubtrees  = ntoh_l(ondisk->nsubtrees);
	de->de_nfiles     = ntoh_l(ondisk->nfiles);
	de->de_nentries   = ntoh_l(ondisk->nentries);
	de->de_pathlen    = len;
	hashcpy(de->sha1, ondisk->sha1);
	return de;
}

static struct directory_entry *read_directories(unsigned int *dir_offset,
				unsigned int *dir_table_offset,
				void *mmap,
				int mmap_size)
{
	int i, ondisk_directory_size;
	uint32_t *filecrc, *beginning, *end;
	struct ondisk_directory_entry *disk_de;
	struct directory_entry *de;
	unsigned int data_len, len;
	char *name;

	/*
	 * Length of pathname + nul byte for termination + size of
	 * members of ondisk_directory_entry. (Just using the size
	 * of the struct doesn't work, because there may be padding
	 * bytes for the struct)
	 */
	ondisk_directory_size = sizeof(disk_de->flags)
		+ sizeof(disk_de->foffset)
		+ sizeof(disk_de->cr)
		+ sizeof(disk_de->ncr)
		+ sizeof(disk_de->nsubtrees)
		+ sizeof(disk_de->nfiles)
		+ sizeof(disk_de->nentries)
		+ sizeof(disk_de->sha1);
	name = ptr_add(mmap, *dir_offset);
	beginning = ptr_add(mmap, *dir_table_offset);
	end = ptr_add(mmap, *dir_table_offset + 4);
	len = ntoh_l(*end) - ntoh_l(*beginning) - ondisk_directory_size - 5;
	disk_de = ptr_add(mmap, *dir_offset + len + 1);
	de = directory_entry_from_ondisk(disk_de, name, len);
	de->next = NULL;
	de->sub = NULL;

	data_len = len + 1 + ondisk_directory_size;
	filecrc = ptr_add(mmap, *dir_offset + data_len);
	if (!check_crc32(0, ptr_add(mmap, *dir_offset), data_len, ntoh_l(*filecrc)))
		die("directory crc doesn't match for '%s'", de->pathname);

	*dir_table_offset += 4;
	*dir_offset += data_len + 4; /* crc code */

	de->sub = xcalloc(de->de_nsubtrees, sizeof(struct directory_entry *));
	for (i = 0; i < de->de_nsubtrees; i++) {
		de->sub[i] = read_directories(dir_offset, dir_table_offset,
						   mmap, mmap_size);
	}

	return de;
}

static int read_entry(struct cache_entry **ce, char *pathname, size_t pathlen,
		      void *mmap, unsigned long mmap_size,
		      unsigned int first_entry_offset,
		      unsigned int foffsetblock)
{
	int len, offset_to_offset;
	char *name;
	uint32_t foffsetblockcrc, *filecrc, *beginning, *end, entry_offset;
	struct ondisk_cache_entry *disk_ce;

	beginning = ptr_add(mmap, foffsetblock);
	end = ptr_add(mmap, foffsetblock + 4);
	len = ntoh_l(*end) - ntoh_l(*beginning) - sizeof(struct ondisk_cache_entry) - 5;
	entry_offset = first_entry_offset + ntoh_l(*beginning);
	name = ptr_add(mmap, entry_offset);
	disk_ce = ptr_add(mmap, entry_offset + len + 1);
	*ce = cache_entry_from_ondisk(disk_ce, pathname, name, len, pathlen);
	filecrc = ptr_add(mmap, entry_offset + len + 1 + sizeof(*disk_ce));
	offset_to_offset = htonl(foffsetblock);
	foffsetblockcrc = crc32(0, (Bytef*)&offset_to_offset, 4);
	if (!check_crc32(foffsetblockcrc,
		ptr_add(mmap, entry_offset), len + 1 + sizeof(*disk_ce),
		ntoh_l(*filecrc)))
		return -1;

	return 0;
}

static struct conflict_part *conflict_part_from_ondisk(struct ondisk_conflict_part *ondisk)
{
	struct conflict_part *cp = xmalloc(sizeof(struct conflict_part));

	cp->flags      = ntoh_s(ondisk->flags);
	cp->entry_mode = ntoh_s(ondisk->entry_mode);
	hashcpy(cp->sha1, ondisk->sha1);
	return cp;
}

static void conflict_entry_push(struct conflict_entry **head,
				struct conflict_entry **tail,
				struct conflict_entry *conflict_entry)
{
	if (!*head) {
		*head = *tail = conflict_entry;
		(*tail)->next = NULL;
		return;
	}

	(*tail)->next = conflict_entry;
	conflict_entry->next = NULL;
	*tail = (*tail)->next;
}

struct conflict_entry *create_new_conflict(char *name, int len, int pathlen)
{
	struct conflict_entry *conflict_entry;

	if (pathlen)
		pathlen++;
	conflict_entry = xmalloc(conflict_entry_size(len));
	conflict_entry->entries = NULL;
	conflict_entry->nfileconflicts = 0;
	conflict_entry->namelen = len;
	memcpy(conflict_entry->name, name, len);
	conflict_entry->name[len] = '\0';
	conflict_entry->pathlen = pathlen;
	conflict_entry->next = NULL;

	return conflict_entry;
}

void add_part_to_conflict_entry(struct directory_entry *de,
					struct conflict_entry *entry,
					struct conflict_part *conflict_part)
{

	struct conflict_part *conflict_search;

	entry->nfileconflicts++;
	de->conflict_size += sizeof(struct ondisk_conflict_part);
	if (!entry->entries)
		entry->entries = conflict_part;
	else {
		conflict_search = entry->entries;
		while (conflict_search->next)
			conflict_search = conflict_search->next;
		conflict_search->next = conflict_part;
	}
}

static int read_conflicts(struct conflict_entry **head,
			  struct directory_entry *de,
			  void *mmap, unsigned long mmap_size)
{
	struct conflict_entry *tail;
	unsigned int croffset, i;
	char *full_name;

	croffset = de->de_cr;
	tail = NULL;
	for (i = 0; i < de->de_ncr; i++) {
		struct conflict_entry *conflict_new;
		unsigned int len, *nfileconflicts;
		char *name;
		void *crc_start;
		int k, offset;
		uint32_t *filecrc;

		offset = croffset;
		crc_start = ptr_add(mmap, offset);
		name = ptr_add(mmap, offset);
		len = strlen(name);
		offset += len + 1;
		nfileconflicts = ptr_add(mmap, offset);
		offset += 4;

		full_name = xmalloc(sizeof(char) * (len + de->de_pathlen));
		memcpy(full_name, de->pathname, de->de_pathlen);
		memcpy(full_name + de->de_pathlen, name, len);
		conflict_new = create_new_conflict(full_name,
				len + de->de_pathlen, de->de_pathlen);
		for (k = 0; k < ntoh_l(*nfileconflicts); k++) {
			struct ondisk_conflict_part *ondisk;
			struct conflict_part *cp;

			ondisk = ptr_add(mmap, offset);
			cp = conflict_part_from_ondisk(ondisk);
			cp->next = NULL;
			add_part_to_conflict_entry(de, conflict_new, cp);
			offset += sizeof(struct ondisk_conflict_part);
		}
		filecrc = ptr_add(mmap, offset);
		free(full_name);
		if (!check_crc32(0, crc_start,
			len + 1 + 4 + conflict_new->nfileconflicts
			* sizeof(struct ondisk_conflict_part),
			ntoh_l(*filecrc)))
			return -1;
		croffset = offset + 4;
		conflict_entry_push(head, &tail, conflict_new);
	}
	return 0;
}

static int convert_resolve_undo(struct index_state *istate,
				struct directory_entry *de,
				void *mmap, unsigned long mmap_size)
{
	int i;
	struct conflict_entry *conflicts = NULL;

	if (read_conflicts(&conflicts, de, mmap, mmap_size) < 0)
		return -1;

	while (conflicts) {
		struct string_list_item *lost;
		struct resolve_undo_info *ui;
		struct conflict_part *cp;

		if (conflicts->entries &&
		    (conflicts->entries->flags & CONFLICT_CONFLICTED)) {
			conflicts = conflicts->next;
			continue;
		}
		if (!istate->resolve_undo) {
			istate->resolve_undo = xcalloc(1, sizeof(struct string_list));
			istate->resolve_undo->strdup_strings = 1;
		}

		lost = string_list_insert(istate->resolve_undo, conflicts->name);
		if (!lost->util)
			lost->util = xcalloc(1, sizeof(*ui));
		ui = lost->util;

		cp = conflicts->entries;
		for (i = 0; i < 3; i++)
			ui->mode[i] = 0;
		while (cp) {
			ui->mode[conflict_stage(cp) - 1] = cp->entry_mode;
			hashcpy(ui->sha1[conflict_stage(cp) - 1], cp->sha1);
			cp = cp->next;
		}
		conflicts = conflicts->next;
	}
	for (i = 0; i < de->de_nsubtrees; i++)
		if (convert_resolve_undo(istate, de->sub[i], mmap, mmap_size) < 0)
			return -1;
	return 0;
}

static struct cache_tree *convert_one(struct directory_entry *de)
{
	int i;
	struct cache_tree *it;

	it = cache_tree();
	it->entry_count = de->de_nentries;
	if (0 <= it->entry_count)
		hashcpy(it->sha1, de->sha1);

	/*
	 * Just a heuristic -- we do not add directories that often but
	 * we do not want to have to extend it immediately when we do,
	 * hence +2.
	 */
	it->subtree_alloc = de->de_nsubtrees + 2;
	it->down = xcalloc(it->subtree_alloc, sizeof(struct cache_tree_sub *));
	for (i = 0; i < de->de_nsubtrees; i++) {
		struct cache_tree *sub;
		struct cache_tree_sub *subtree;
		char *buf, *name;

		sub = convert_one(de->sub[i]);
		if(!sub)
			goto free_return;

		name = "";
		buf = strtok(de->sub[i]->pathname, "/");
		while (buf) {
			name = buf;
			buf = strtok(NULL, "/");
		}
		subtree = cache_tree_sub(it, name);
		subtree->cache_tree = sub;
	}
	if (de->de_nsubtrees != it->subtree_nr)
		die("cache-tree: internal error");
	return it;
 free_return:
	cache_tree_free(&it);
	return NULL;
}

static int compare_cache_tree_elements(const void *a, const void *b)
{
	const struct directory_entry *de1, *de2;

	de1 = (const struct directory_entry *) a;
	de2 = (const struct directory_entry *) b;
	return subtree_name_cmp(de1->pathname, de1->de_pathlen,
				de2->pathname, de2->de_pathlen);
}

static void sort_directories(struct directory_entry *de)
{
	int i;

	for (i = 0; i < de->de_nsubtrees; i++) {
		if (de->sub[i]->de_nsubtrees)
			sort_directories(de->sub[i]);
	}
	qsort(de->sub, de->de_nsubtrees, sizeof(struct directory_entry *),
	      compare_cache_tree_elements);
}

/*
 * This function modifies the directory argument that is given to it.
 * Don't use it if the directory entries are still needed after.
 */
static struct cache_tree *cache_tree_convert_v5(struct directory_entry *de)
{
	if (!de->de_nentries)
		return NULL;
	sort_directories(de);
	return convert_one(de);
}

static int read_entries(struct index_state *istate, struct directory_entry *de,
			unsigned int first_entry_offset, void *mmap,
			unsigned long mmap_size, unsigned int *nr,
			unsigned int foffsetblock)
{
	struct cache_entry *ce;
	int i, subdir = 0;

	for (i = 0; i < de->de_nfiles; i++) {
		unsigned int subdir_foffsetblock = de->de_foffset + foffsetblock + (i * 4);
		if (read_entry(&ce, de->pathname, de->de_pathlen, mmap, mmap_size,
			       first_entry_offset, subdir_foffsetblock) < 0)
			return -1;
		while (subdir < de->de_nsubtrees &&
		       cache_name_compare(ce->name + de->de_pathlen,
					  ce_namelen(ce) - de->de_pathlen,
					  de->sub[subdir]->pathname + de->de_pathlen,
					  de->sub[subdir]->de_pathlen - de->de_pathlen) > 0) {
			read_entries(istate, de->sub[subdir], first_entry_offset, mmap,
				     mmap_size, nr, foffsetblock);
			subdir++;
		}
		if (!ce)
			continue;
		set_index_entry(istate, (*nr)++, ce);
	}
	for (i = subdir; i < de->de_nsubtrees; i++) {
		read_entries(istate, de->sub[i], first_entry_offset, mmap,
			     mmap_size, nr, foffsetblock);
	}
	return 0;
}

static struct directory_entry *read_all_directories(struct index_state *istate,
						    unsigned int *entry_offset,
						    unsigned int *foffsetblock,
						    unsigned int *ndirs,
						    void *mmap, unsigned long mmap_size)
{
	unsigned int dir_offset, dir_table_offset;
	struct cache_header *hdr;
	struct cache_header_v5 *hdr_v5;
	struct directory_entry *root_directory;

	hdr = mmap;
	hdr_v5 = ptr_add(mmap, sizeof(*hdr));
	istate->cache_alloc = alloc_nr(ntohl(hdr->hdr_entries));
	istate->cache = xcalloc(istate->cache_alloc, sizeof(struct cache_entry *));

	/* Skip size of the header + crc sum + size of offsets to extensions + size of offsets */
	dir_offset = sizeof(*hdr) + sizeof(*hdr_v5) + ntohl(hdr_v5->hdr_nextension) * 4 + 4
		+ (ntohl(hdr_v5->hdr_ndir) + 1) * 4;
	dir_table_offset = sizeof(*hdr) + sizeof(*hdr_v5) + ntohl(hdr_v5->hdr_nextension) * 4 + 4;
	root_directory = read_directories(&dir_offset, &dir_table_offset,
					  mmap, mmap_size);

	*entry_offset = ntohl(hdr_v5->hdr_fblockoffset);
	*foffsetblock = dir_offset;
	*ndirs = ntohl(hdr_v5->hdr_ndir);
	return root_directory;
}

static int read_index_v5(struct index_state *istate, void *mmap,
			 unsigned long mmap_size, struct filter_opts *opts)
{
	unsigned int entry_offset, ndirs, foffsetblock, nr = 0;
	struct directory_entry *root_directory, *de, *last_de;
	const char **paths = NULL;
	struct pathspec adjusted_pathspec;
	int need_root = 0, i;

	root_directory = read_all_directories(istate, &entry_offset,
					      &foffsetblock, &ndirs,
					      mmap, mmap_size);

	if (opts && opts->pathspec && opts->pathspec->nr) {
		need_root = 0;
		paths = xmalloc((opts->pathspec->nr + 1)*sizeof(char *));
		paths[opts->pathspec->nr] = NULL;
		for (i = 0; i < opts->pathspec->nr; i++) {
			char *super = strdup(opts->pathspec->items[i].match);
			int len = strlen(super);
			while (len && super[len - 1] == '/' && super[len - 2] == '/')
				super[--len] = '\0'; /* strip all but one trailing slash */
			while (len && super[--len] != '/')
				; /* scan backwards to next / */
			if (len >= 0)
				super[len--] = '\0';
			if (len <= 0) {
				need_root = 1;
				break;
			}
			paths[i] = super;
		}
	}

	if (!need_root)
		parse_pathspec(&adjusted_pathspec, PATHSPEC_ALL_MAGIC, PATHSPEC_PREFER_CWD, NULL, paths);

	if (!opts || opts->read_resolve_undo)
		if (convert_resolve_undo(istate, root_directory, mmap, mmap_size) < 0)
			return -1;

	de = root_directory;
	last_de = de;
	while (de) {
		if (need_root ||
		    match_pathspec_depth(&adjusted_pathspec, de->pathname, de->de_pathlen, 0, NULL)) {
			if (read_entries(istate, de, entry_offset,
					 mmap, mmap_size, &nr,
					 foffsetblock) < 0)
				return -1;
		} else {
			for (i = 0; i < de->de_nsubtrees; i++) {
				last_de->next = de->sub[i];
				last_de = last_de->next;
			}
		}
		de = de->next;
	}
	istate->cache_tree = cache_tree_convert_v5(root_directory);
	istate->cache_nr = nr;
	return 0;
}

struct index_ops v5_ops = {
	match_stat_basic,
	verify_hdr,
	read_index_v5,
	NULL
};
