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

#define WRITE_BUFFER_SIZE 8192
static unsigned char write_buffer[WRITE_BUFFER_SIZE];
static unsigned long write_buffer_len;

static int ce_write_flush(int fd)
{
	unsigned int buffered = write_buffer_len;
	if (buffered) {
		if (write_in_full(fd, write_buffer, buffered) != buffered)
			return -1;
		write_buffer_len = 0;
	}
	return 0;
}

static int ce_write(uint32_t *crc, int fd, void *data, unsigned int len)
{
	if (crc)
		*crc = crc32(*crc, (Bytef*)data, len);
	while (len) {
		unsigned int buffered = write_buffer_len;
		unsigned int partial = WRITE_BUFFER_SIZE - buffered;
		if (partial > len)
			partial = len;
		memcpy(write_buffer + buffered, data, partial);
		buffered += partial;
		if (buffered == WRITE_BUFFER_SIZE) {
			write_buffer_len = buffered;
			if (ce_write_flush(fd))
				return -1;
			buffered = 0;
		}
		write_buffer_len = buffered;
		len -= partial;
		data = (char *) data + partial;
	}
	return 0;
}

static int ce_flush(int fd)
{
	unsigned int left = write_buffer_len;

	if (left)
		write_buffer_len = 0;

	if (write_in_full(fd, write_buffer, left) != left)
		return -1;

	return 0;
}

static void ce_smudge_racily_clean_entry(struct cache_entry *ce)
{
	/*
	 * This method shall only be called if the timestamp of ce
	 * is racy (check with is_racy_timestamp). If the timestamp
	 * is racy, the writer will set the CE_SMUDGED flag.
	 *
	 * The reader (match_stat_basic) will then take care
	 * of checking if the entry is really changed or not, by
	 * taking into account the size and the stat_crc and if
	 * that hasn't changed checking the sha1.
	 */
	ce->ce_flags |= CE_SMUDGED;
}

char *super_directory(const char *filename)
{
	char *slash;

	slash = strrchr(filename, '/');
	if (slash)
		return xmemdupz(filename, slash-filename);
	return NULL;
}

struct directory_entry *init_directory_entry(char *pathname, int len)
{
	struct directory_entry *de = xmalloc(directory_entry_size(len));

	memcpy(de->pathname, pathname, len);
	de->pathname[len] = '\0';
	de->de_flags      = 0;
	de->de_foffset    = 0;
	de->de_cr         = 0;
	de->de_ncr        = 0;
	de->de_nsubtrees  = 0;
	de->de_nfiles     = 0;
	de->de_nentries   = 0;
	memset(de->sha1, 0, 20);
	de->de_pathlen    = len;
	de->next          = NULL;
	de->next_hash     = NULL;
	de->ce            = NULL;
	de->ce_last       = NULL;
	de->conflict      = NULL;
	de->conflict_last = NULL;
	de->conflict_size = 0;
	return de;
}

static void ondisk_from_directory_entry(struct directory_entry *de,
					struct ondisk_directory_entry *ondisk)
{
	ondisk->foffset   = htonl(de->de_foffset);
	ondisk->cr        = htonl(de->de_cr);
	ondisk->ncr       = htonl(de->de_ncr);
	ondisk->nsubtrees = htonl(de->de_nsubtrees);
	ondisk->nfiles    = htonl(de->de_nfiles);
	ondisk->nentries  = htonl(de->de_nentries);
	hashcpy(ondisk->sha1, de->sha1);
	ondisk->flags     = htons(de->de_flags);
}

static struct conflict_part *conflict_part_from_inmemory(struct cache_entry *ce)
{
	struct conflict_part *conflict;
	int flags;

	conflict = xmalloc(sizeof(struct conflict_part));
	flags                = CONFLICT_CONFLICTED;
	flags               |= ce_stage(ce) << CONFLICT_STAGESHIFT;
	conflict->flags      = flags;
	conflict->entry_mode = ce->ce_mode;
	conflict->next       = NULL;
	hashcpy(conflict->sha1, ce->sha1);
	return conflict;
}

static void conflict_to_ondisk(struct conflict_part *cp,
				struct ondisk_conflict_part *ondisk)
{
	ondisk->flags      = htons(cp->flags);
	ondisk->entry_mode = htons(cp->entry_mode);
	hashcpy(ondisk->sha1, cp->sha1);
}

void add_conflict_to_directory_entry(struct directory_entry *de,
					struct conflict_entry *conflict_entry)
{
	de->de_ncr++;
	de->conflict_size += conflict_entry->namelen + 1 + 8 - conflict_entry->pathlen;
	conflict_entry_push(&de->conflict, &de->conflict_last, conflict_entry);
}

void insert_directory_entry(struct directory_entry *de,
			struct hash_table *table,
			unsigned int *total_dir_len,
			unsigned int *ndir,
			uint32_t crc)
{
	struct directory_entry *insert;

	insert = (struct directory_entry *)insert_hash(crc, de, table);
	if (insert) {
		de->next_hash = insert->next_hash;
		insert->next_hash = de;
	}
	(*ndir)++;
	if (de->de_pathlen == 0)
		(*total_dir_len)++;
	else
		*total_dir_len += de->de_pathlen + 2;
}

static void add_missing_superdirs(char *dir, struct hash_table *table,
				  unsigned int *total_dir_len,
				  unsigned int *ndir,
				  struct directory_entry *prev,
				  struct directory_entry **current)
{
	struct directory_entry *no_subtrees, *found, *new;
	int dir_len;
	uint32_t crc;

	no_subtrees = *current;
	dir = super_directory(dir);
	dir_len = dir ? strlen(dir) : 0;
	crc = crc32(0, (Bytef*)dir, dir_len);
	found = lookup_hash(crc, table);
	while (!found) {
		new = init_directory_entry(dir, dir_len);
		new->de_nsubtrees = 1;
		new->next = no_subtrees;
		no_subtrees = new;
		insert_directory_entry(new, table, total_dir_len, ndir, crc);
		dir = super_directory(dir);
		dir_len = dir ? strlen(dir) : 0;
		crc = crc32(0, (Bytef*)dir, dir_len);
		found = lookup_hash(crc, table);
	}
	while (found->next_hash && strcmp(dir, found->pathname) != 0)
		found = found->next_hash;
	found->de_nsubtrees++;
	prev->next = no_subtrees;
}

static struct directory_entry *get_directory(char *dir, unsigned int dir_len,
					     struct hash_table *table,
					     unsigned int *total_dir_len,
					     unsigned int *ndir,
					     struct directory_entry **current)
{
	struct directory_entry *search, *found, *new, *prev;
	uint32_t crc;

	crc = crc32(0, (Bytef*)dir, dir_len);
	found = lookup_hash(crc, table);
	search = found;
	while (search && dir_len &&
	       cache_name_compare(dir, dir_len, search->pathname, search->de_pathlen))
		search = search->next_hash;

	prev = *current;
	if (!search) {
		new = init_directory_entry(dir, dir_len);
		search = new;
		(*current)->next = new;
		*current = (*current)->next;
		insert_directory_entry(new, table, total_dir_len, ndir, crc);
	}

	/*
	 * Index-v5 stores all the directories, even if there are only
	 * only subdirectories in a specific directory, but no files.
	 */
	if (dir && !found)
		add_missing_superdirs(dir, table, total_dir_len, ndir, prev, current);

	return search;
}

static struct conflict_entry *create_conflict_entry_from_ce(struct cache_entry *ce,
								int pathlen)
{
	return create_new_conflict(ce->name, ce_namelen(ce), pathlen);
}

static void convert_one_to_ondisk_v5(struct hash_table *table, struct cache_tree *it,
				const char *path, int pathlen, uint32_t crc)
{
	int i;
	struct directory_entry *found, *search;

	crc = crc32(crc, (Bytef*)path, pathlen);
	found = lookup_hash(crc, table);
	search = found;
	while (search && strcmp(path, search->pathname + search->de_pathlen - strlen(path)) != 0)
		search = search->next_hash;
	if (!search)
		return;
	/*
	 * The number of subtrees is already calculated by
	 * compile_directory_data, therefore we only need to
	 * add the entry_count
	 */
	search->de_nentries = it->entry_count;
	if (0 <= it->entry_count)
		hashcpy(search->sha1, it->sha1);
	if (strcmp(path, "") != 0)
		crc = crc32(crc, (Bytef*)"/", 1);

#if DEBUG
	if (0 <= it->entry_count)
		fprintf(stderr, "cache-tree <%.*s> (%d ent, %d subtree) %s\n",
			pathlen, path, it->entry_count, it->subtree_nr,
			sha1_to_hex(it->sha1));
	else
		fprintf(stderr, "cache-tree <%.*s> (%d subtree) invalid\n",
			pathlen, path, it->subtree_nr);
#endif

	for (i = 0; i < it->subtree_nr; i++) {
		struct cache_tree_sub *down = it->down[i];
		if (i) {
			struct cache_tree_sub *prev = it->down[i-1];
			if (subtree_name_cmp(down->name, down->namelen,
					     prev->name, prev->namelen) <= 0)
				die("fatal - unsorted cache subtree");
		}
		convert_one_to_ondisk_v5(table, down->cache_tree, down->name, down->namelen, crc);
	}
}

static void cache_tree_to_ondisk_v5(struct hash_table *table, struct cache_tree *root)
{
	convert_one_to_ondisk_v5(table, root, "", 0, 0);
}

static void ce_queue_push(struct cache_entry **head,
			  struct cache_entry **tail,
			  struct cache_entry *ce)
{
	if (!*head) {
		*head = *tail = ce;
		(*tail)->next_ce = NULL;
		return;
	}

	(*tail)->next_ce = ce;
	ce->next_ce = NULL;
	*tail = (*tail)->next_ce;
}

static struct directory_entry *compile_directory_data(struct index_state *istate,
						      int nfile,
						      unsigned int *ndir,
						      unsigned int *total_dir_len,
						      unsigned int *total_file_len)
{
	int i, dir_len = -1;
	char *dir;
	struct directory_entry *de, *current, *search;
	struct cache_entry **cache = istate->cache;
	struct conflict_entry *conflict_entry;
	struct hash_table table;
	uint32_t crc;

	init_hash(&table);
	de = init_directory_entry("", 0);
	current = de;
	*ndir = 1;
	*total_dir_len = 1;
	crc = crc32(0, (Bytef*)de->pathname, de->de_pathlen);
	insert_hash(crc, de, &table);
	conflict_entry = NULL;
	for (i = 0; i < nfile; i++) {
		if (cache[i]->ce_flags & CE_REMOVE)
			continue;

		if (dir_len < 0
		    || cache[i]->name[dir_len] != '/'
		    || strchr(cache[i]->name + dir_len + 1, '/')
		    || cache_name_compare(cache[i]->name, ce_namelen(cache[i]),
					  dir, dir_len)) {
			dir = super_directory(cache[i]->name);
			dir_len = dir ? strlen(dir) : 0;
			search = get_directory(dir, dir_len, &table,
					       total_dir_len, ndir,
					       &current);
		}
		search->de_nfiles++;
		*total_file_len += ce_namelen(cache[i]) + 1;
		if (search->de_pathlen)
			*total_file_len -= search->de_pathlen + 1;
		ce_queue_push(&(search->ce), &(search->ce_last), cache[i]);

		if (ce_stage(cache[i]) > 0) {
			struct conflict_part *conflict_part;
			if (!conflict_entry ||
			    cache_name_compare(conflict_entry->name, conflict_entry->namelen,
					       cache[i]->name, ce_namelen(cache[i]))) {
				conflict_entry = create_conflict_entry_from_ce(cache[i], search->de_pathlen);
				add_conflict_to_directory_entry(search, conflict_entry);
			}
			conflict_part = conflict_part_from_inmemory(cache[i]);
			add_part_to_conflict_entry(search, conflict_entry, conflict_part);
		}
	}
	if (istate->cache_tree)
		cache_tree_to_ondisk_v5(&table, istate->cache_tree);
	return de;
}

static void ondisk_from_cache_entry(struct cache_entry *ce,
				    struct ondisk_cache_entry *ondisk)
{
	unsigned int flags;

	flags  = ce->ce_flags & CE_STAGEMASK;
	flags |= ce->ce_flags & CE_VALID;
	flags |= ce->ce_flags & CE_SMUDGED;
	if (ce->ce_flags & CE_INTENT_TO_ADD)
		flags |= CE_INTENT_TO_ADD_V5;
	if (ce->ce_flags & CE_SKIP_WORKTREE)
		flags |= CE_SKIP_WORKTREE_V5;
	ondisk->flags      = htons(flags);
	ondisk->mode       = htons(ce->ce_mode);
	ondisk->mtime.sec  = htonl(ce->ce_stat_data.sd_mtime.sec);
#ifdef USE_NSEC
	ondisk->mtime.nsec = htonl(ce->ce_stat_data.sd_mtime.nsec);
#else
	ondisk->mtime.nsec = 0;
#endif
	ondisk->size       = htonl(ce->ce_stat_data.sd_size);
	if (!ce->ce_stat_crc)
		ce->ce_stat_crc = calculate_stat_crc(ce);
	ondisk->stat_crc   = htonl(ce->ce_stat_crc);
	hashcpy(ondisk->sha1, ce->sha1);
}

static int write_directories(struct directory_entry *de, int fd, int conflict_offset)
{
	struct directory_entry *current;
	struct ondisk_directory_entry ondisk;
	int current_offset, offset_write, ondisk_size, foffset;
	uint32_t crc;

	/*
	 * This is needed because the compiler aligns structs to sizes multiple
	 * of 4
	 */
	ondisk_size = sizeof(ondisk.flags)
		+ sizeof(ondisk.foffset)
		+ sizeof(ondisk.cr)
		+ sizeof(ondisk.ncr)
		+ sizeof(ondisk.nsubtrees)
		+ sizeof(ondisk.nfiles)
		+ sizeof(ondisk.nentries)
		+ sizeof(ondisk.sha1);
	current = de;
	current_offset = 0;
	foffset = 0;
	while (current) {
		int pathlen;

		offset_write = htonl(current_offset);
		if (ce_write(NULL, fd, &offset_write, 4) < 0)
			return -1;
		if (current->de_pathlen == 0)
			pathlen = 0;
		else
			pathlen = current->de_pathlen + 1;
		current_offset += pathlen + 1 + ondisk_size + 4;
		current = current->next;
	}
	/*
	 * Write one more offset, which points to the end of the entries,
	 * because we use it for calculating the dir length, instead of
	 * using strlen.
	 */
	offset_write = htonl(current_offset);
	if (ce_write(NULL, fd, &offset_write, 4) < 0)
		return -1;
	current = de;
	while (current) {
		crc = 0;
		if (current->de_pathlen == 0) {
			if (ce_write(&crc, fd, current->pathname, 1) < 0)
				return -1;
		} else {
			char *path;
			path = xmalloc(sizeof(char) * (current->de_pathlen + 2));
			memcpy(path, current->pathname, current->de_pathlen);
			memcpy(path + current->de_pathlen, "/\0", 2);
			if (ce_write(&crc, fd, path, current->de_pathlen + 2) < 0)
				return -1;
		}
		current->de_foffset = foffset;
		current->de_cr = conflict_offset;
		ondisk_from_directory_entry(current, &ondisk);
		if (ce_write(&crc, fd, &ondisk, ondisk_size) < 0)
			return -1;
		crc = htonl(crc);
		if (ce_write(NULL, fd, &crc, 4) < 0)
			return -1;
		conflict_offset += current->conflict_size;
		foffset += current->de_nfiles * 4;
		current = current->next;
	}
	return 0;
}

static int write_entries(struct index_state *istate,
			    struct directory_entry *de,
			    int entries,
			    int fd,
			    int offset_to_offset)
{
	int offset, offset_write, ondisk_size;
	struct directory_entry *current;

	offset = 0;
	ondisk_size = sizeof(struct ondisk_cache_entry);
	current = de;
	while (current) {
		int pathlen;
		struct cache_entry *ce = current->ce;

		if (current->de_pathlen == 0)
			pathlen = 0;
		else
			pathlen = current->de_pathlen + 1;
		while (ce) {
			if (ce->ce_flags & CE_REMOVE)
				continue;
			if (!ce_uptodate(ce) && is_racy_timestamp(istate, ce))
				ce_smudge_racily_clean_entry(ce);
			if (is_null_sha1(ce->sha1))
				return error("cache entry has null sha1: %s", ce->name);

			offset_write = htonl(offset);
			if (ce_write(NULL, fd, &offset_write, 4) < 0)
				return -1;
			offset += ce_namelen(ce) - pathlen + 1 + ondisk_size + 4;
			ce = ce->next_ce;
		}
		current = current->next;
	}
	/*
	 * Write one more offset, which points to the end of the entries,
	 * because we use it for calculating the file length, instead of
	 * using strlen.
	 */
	offset_write = htonl(offset);
	if (ce_write(NULL, fd, &offset_write, 4) < 0)
		return -1;

	offset = offset_to_offset;
	current = de;
	while (current) {
		int pathlen;
		struct cache_entry *ce = current->ce;

		if (current->de_pathlen == 0)
			pathlen = 0;
		else
			pathlen = current->de_pathlen + 1;
		while (ce) {
			struct ondisk_cache_entry ondisk;
			uint32_t crc, calc_crc;

			if (ce->ce_flags & CE_REMOVE)
				continue;
			calc_crc = htonl(offset);
			crc = crc32(0, (Bytef*)&calc_crc, 4);
			if (ce_write(&crc, fd, ce->name + pathlen,
					ce_namelen(ce) - pathlen + 1) < 0)
				return -1;
			ondisk_from_cache_entry(ce, &ondisk);
			if (ce_write(&crc, fd, &ondisk, ondisk_size) < 0)
				return -1;
			crc = htonl(crc);
			if (ce_write(NULL, fd, &crc, 4) < 0)
				return -1;
			offset += 4;
			ce = ce->next_ce;
		}
		current = current->next;
	}
	return 0;
}

static int write_conflict(struct conflict_entry *conflict, int fd)
{
	struct conflict_entry *current;
	struct conflict_part *current_part;
	uint32_t crc;

	current = conflict;
	while (current) {
		unsigned int to_write;

		crc = 0;
		if (ce_write(&crc, fd,
		     (Bytef*)(current->name + current->pathlen),
		     current->namelen - current->pathlen) < 0)
			return -1;
		if (ce_write(&crc, fd, (Bytef*)"\0", 1) < 0)
			return -1;
		to_write = htonl(current->nfileconflicts);
		if (ce_write(&crc, fd, (Bytef*)&to_write, 4) < 0)
			return -1;
		current_part = current->entries;
		while (current_part) {
			struct ondisk_conflict_part ondisk;

			conflict_to_ondisk(current_part, &ondisk);
			if (ce_write(&crc, fd, (Bytef*)&ondisk, sizeof(struct ondisk_conflict_part)) < 0)
				return 0;
			current_part = current_part->next;
		}
		to_write = htonl(crc);
		if (ce_write(NULL, fd, (Bytef*)&to_write, 4) < 0)
			return -1;
		current = current->next;
	}
	return 0;
}

static int write_conflicts(struct index_state *istate,
			      struct directory_entry *de,
			      int fd)
{
	struct directory_entry *current;

	current = de;
	while (current) {
		if (current->de_ncr != 0) {
			if (write_conflict(current->conflict, fd) < 0)
				return -1;
		}
		current = current->next;
	}
	return 0;
}

static int write_index_v5(struct index_state *istate, int newfd)
{
	struct cache_header hdr;
	struct cache_header_v5 hdr_v5;
	struct cache_entry **cache = istate->cache;
	struct directory_entry *de;
	struct ondisk_directory_entry *ondisk;
	unsigned int entries = istate->cache_nr;
	unsigned int i, removed, total_dir_len, ondisk_directory_size;
	unsigned int total_file_len, conflict_offset, foffsetblock;
	unsigned int ndir;
	uint32_t crc;

	if (istate->filter_opts)
		die("BUG: index: cannot write a partially read index");

	for (i = removed = 0; i < entries; i++) {
		if (cache[i]->ce_flags & CE_REMOVE)
			removed++;
	}
	hdr.hdr_signature = htonl(CACHE_SIGNATURE);
	hdr.hdr_version = htonl(istate->version);
	hdr.hdr_entries = htonl(entries - removed);
	hdr_v5.hdr_nextension = htonl(0); /* Currently no extensions are supported */

	total_dir_len = 0;
	total_file_len = 0;
	de = compile_directory_data(istate, entries, &ndir,
				    &total_dir_len, &total_file_len);
	hdr_v5.hdr_ndir = htonl(ndir);

	/*
	 * This is needed because the compiler aligns structs to sizes multipe
	 * of 4
	 */
	ondisk_directory_size = sizeof(ondisk->flags)
		+ sizeof(ondisk->foffset)
		+ sizeof(ondisk->cr)
		+ sizeof(ondisk->ncr)
		+ sizeof(ondisk->nsubtrees)
		+ sizeof(ondisk->nfiles)
		+ sizeof(ondisk->nentries)
		+ sizeof(ondisk->sha1);
	foffsetblock = sizeof(hdr) + sizeof(hdr_v5) + 4
		+ (ndir + 1) * 4
		+ total_dir_len
		+ ndir * (ondisk_directory_size + 4);
	hdr_v5.hdr_fblockoffset = htonl(foffsetblock + (entries - removed + 1) * 4);
	crc = 0;
	if (ce_write(&crc, newfd, &hdr, sizeof(hdr)) < 0)
		return -1;
	if (ce_write(&crc, newfd, &hdr_v5, sizeof(hdr_v5)) < 0)
		return -1;
	crc = htonl(crc);
	if (ce_write(NULL, newfd, &crc, 4) < 0)
		return -1;

	conflict_offset = foffsetblock +
		+ (entries - removed + 1) * 4
		+ total_file_len
		+ (entries - removed) * (sizeof(struct ondisk_cache_entry) + 4);
	if (write_directories(de, newfd, conflict_offset) < 0)
		return -1;
	if (write_entries(istate, de, entries, newfd, foffsetblock) < 0)
		return -1;
	if (write_conflicts(istate, de, newfd) < 0)
		return -1;
	return ce_flush(newfd);
}

struct index_ops v5_ops = {
	match_stat_basic,
	verify_hdr,
	read_index_v5,
	write_index_v5
};
