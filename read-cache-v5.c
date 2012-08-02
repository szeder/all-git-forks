#include "cache.h"
#include "read-cache.h"
#include "string-list.h"
#include "resolve-undo.h"
#include "cache-tree.h"
#include "dir.h"

#define ptr_add(x,y) ((void *)(((char *)(x)) + (y)))

struct cache_header {
	unsigned int hdr_ndir;
	unsigned int hdr_nfile;
	unsigned int hdr_fblockoffset;
	unsigned int hdr_nextension;
};

/*****************************************************************
 * Index File I/O
 *****************************************************************/

struct ondisk_cache_entry {
	unsigned short flags;
	unsigned short mode;
	struct cache_time mtime;
	unsigned int size;
	int stat_crc;
	unsigned char sha1[20];
};

struct ondisk_directory_entry {
	unsigned int foffset;
	unsigned int cr;
	unsigned int ncr;
	unsigned int nsubtrees;
	unsigned int nfiles;
	unsigned int nentries;
	unsigned char sha1[20];
	unsigned short flags;
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
	struct cache_version_header *hdr;
	struct cache_header *hdr_v5;

	if (size < sizeof(struct cache_version_header)
			+ sizeof (struct cache_header) + 4)
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
						   struct directory_entry *de,
						   char *name,
						   size_t len,
						   size_t prefix_len)
{
	struct cache_entry *ce = xmalloc(cache_entry_size(len + de->de_pathlen));
	int flags;

	flags = ntoh_s(ondisk->flags);
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
	ce->ce_namelen    = len + de->de_pathlen;
	hashcpy(ce->sha1, ondisk->sha1);
	memcpy(ce->name, de->pathname, de->de_pathlen);
	memcpy(ce->name + de->de_pathlen, name, len);
	ce->name[len + de->de_pathlen] = '\0';
	ce->next_ce = NULL;
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

static struct conflict_part *conflict_part_from_ondisk(struct ondisk_conflict_part *ondisk)
{
	struct conflict_part *cp = xmalloc(sizeof(struct conflict_part));

	cp->flags      = ntoh_s(ondisk->flags);
	cp->entry_mode = ntoh_s(ondisk->entry_mode);
	hashcpy(cp->sha1, ondisk->sha1);
	return cp;
}

static struct cache_entry *convert_conflict_part(struct conflict_part *cp,
						char * name,
						unsigned int len)
{

	struct cache_entry *ce = xmalloc(cache_entry_size(len));

	ce->ce_stat_data.sd_ctime.sec  = 0;
	ce->ce_stat_data.sd_mtime.sec  = 0;
	ce->ce_stat_data.sd_ctime.nsec = 0;
	ce->ce_stat_data.sd_mtime.nsec = 0;
	ce->ce_stat_data.sd_dev        = 0;
	ce->ce_stat_data.sd_ino        = 0;
	ce->ce_stat_data.sd_uid        = 0;
	ce->ce_stat_data.sd_gid        = 0;
	ce->ce_stat_data.sd_size       = 0;
	ce->ce_mode       = cp->entry_mode;
	ce->ce_flags      = conflict_stage(cp) << CE_STAGESHIFT;
	ce->ce_stat_crc   = 0;
	ce->ce_namelen    = len;
	hashcpy(ce->sha1, cp->sha1);
	memcpy(ce->name, name, len);
	ce->name[len] = '\0';
	return ce;
}

static struct directory_entry *read_directories(unsigned int *dir_offset,
				unsigned int *dir_table_offset,
				void *mmap,
				int mmap_size)
{
	int i, ondisk_directory_size;
	uint32_t *filecrc, *beginning, *end;
	struct directory_entry *current = NULL;
	struct ondisk_directory_entry *disk_de;
	struct directory_entry *de;
	unsigned int data_len, len;
	char *name;

	/* Length of pathname + nul byte for termination + size of
	 * members of ondisk_directory_entry. (Just using the size
	 * of the stuct doesn't work, because there may be padding
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

	data_len = len + 1 + ondisk_directory_size;
	filecrc = ptr_add(mmap, *dir_offset + data_len);
	if (!check_crc32(0, ptr_add(mmap, *dir_offset), data_len, ntoh_l(*filecrc)))
		goto unmap;

	*dir_table_offset += 4;
	*dir_offset += data_len + 4; /* crc code */

	current = de;
	for (i = 0; i < de->de_nsubtrees; i++) {
		current->next = read_directories(dir_offset, dir_table_offset,
						mmap, mmap_size);
		while (current->next)
			current = current->next;
	}

	return de;
unmap:
	munmap(mmap, mmap_size);
	die("directory crc doesn't match for '%s'", de->pathname);
}

static int read_entry(struct cache_entry **ce, struct directory_entry *de,
		      unsigned int *entry_offset,
		      void **mmap, unsigned long mmap_size,
		      unsigned int *foffsetblock)
{
	int len, offset_to_offset;
	char *name;
	uint32_t foffsetblockcrc;
	uint32_t *filecrc, *beginning, *end;
	struct ondisk_cache_entry *disk_ce;

	name = ptr_add(*mmap, *entry_offset);
	beginning = ptr_add(*mmap, *foffsetblock);
	end = ptr_add(*mmap, *foffsetblock + 4);
	len = ntoh_l(*end) - ntoh_l(*beginning) - sizeof(struct ondisk_cache_entry) - 5;
	disk_ce = ptr_add(*mmap, *entry_offset + len + 1);
	*ce = cache_entry_from_ondisk(disk_ce, de, name, len, de->de_pathlen);
	filecrc = ptr_add(*mmap, *entry_offset + len + 1 + sizeof(*disk_ce));
	offset_to_offset = htonl(*foffsetblock);
	foffsetblockcrc = crc32(0, (Bytef*)&offset_to_offset, 4);
	if (!check_crc32(foffsetblockcrc,
		ptr_add(*mmap, *entry_offset), len + 1 + sizeof(*disk_ce),
		ntoh_l(*filecrc)))
		return -1;

	*entry_offset += len + 1 + sizeof(*disk_ce) + 4;
	return 0;
}

static void ce_queue_push(struct cache_entry **head,
			     struct cache_entry **tail,
			     struct cache_entry *ce)
{
	if (!*head) {
		*head = *tail = ce;
		(*tail)->next = NULL;
		return;
	}

	(*tail)->next = ce;
	ce->next = NULL;
	*tail = (*tail)->next;
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

static struct cache_entry *ce_queue_pop(struct cache_entry **head)
{
	struct cache_entry *ce;

	ce = *head;
	*head = (*head)->next;
	return ce;
}

static void conflict_part_head_remove(struct conflict_part **head)
{
	struct conflict_part *to_free;

	to_free = *head;
	*head = (*head)->next;
	free(to_free);
}

static void conflict_entry_head_remove(struct conflict_entry **head)
{
	struct conflict_entry *to_free;

	to_free = *head;
	*head = (*head)->next;
	free(to_free);
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
			  void **mmap, unsigned long mmap_size)
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
		crc_start = ptr_add(*mmap, offset);
		name = ptr_add(*mmap, offset);
		len = strlen(name);
		offset += len + 1;
		nfileconflicts = ptr_add(*mmap, offset);
		offset += 4;

		full_name = xmalloc(sizeof(char) * (len + de->de_pathlen));
		memcpy(full_name, de->pathname, de->de_pathlen);
		memcpy(full_name + de->de_pathlen, name, len);
		conflict_new = create_new_conflict(full_name,
				len + de->de_pathlen, de->de_pathlen);
		for (k = 0; k < ntoh_l(*nfileconflicts); k++) {
			struct ondisk_conflict_part *ondisk;
			struct conflict_part *cp;

			ondisk = ptr_add(*mmap, offset);
			cp = conflict_part_from_ondisk(ondisk);
			cp->next = NULL;
			add_part_to_conflict_entry(de, conflict_new, cp);
			offset += sizeof(struct ondisk_conflict_part);
		}
		filecrc = ptr_add(*mmap, offset);
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

static struct cache_tree *convert_one(struct directory_queue *queue, int dirnr)
{
	int i, subtree_nr;
	struct cache_tree *it;
	struct directory_queue *down;

	it = cache_tree();
	it->entry_count = queue[dirnr].de->de_nentries;
	subtree_nr = queue[dirnr].de->de_nsubtrees;
	if (0 <= it->entry_count)
		hashcpy(it->sha1, queue[dirnr].de->sha1);

	/*
	 * Just a heuristic -- we do not add directories that often but
	 * we do not want to have to extend it immediately when we do,
	 * hence +2.
	 */
	it->subtree_alloc = subtree_nr + 2;
	it->down = xcalloc(it->subtree_alloc, sizeof(struct cache_tree_sub *));
	down = queue[dirnr].down;
	for (i = 0; i < subtree_nr; i++) {
		struct cache_tree *sub;
		struct cache_tree_sub *subtree;
		char *buf, *name;

		name = "";
		buf = strtok(down[i].de->pathname, "/");
		while (buf) {
			name = buf;
			buf = strtok(NULL, "/");
		}
		sub = convert_one(down, i);
		if(!sub)
			goto free_return;
		subtree = cache_tree_sub(it, name);
		subtree->cache_tree = sub;
	}
	if (subtree_nr != it->subtree_nr)
		die("cache-tree: internal error");
	return it;
 free_return:
	cache_tree_free(&it);
	return NULL;
}

static int compare_cache_tree_elements(const void *a, const void *b)
{
	const struct directory_entry *de1, *de2;

	de1 = ((const struct directory_queue *)a)->de;
	de2 = ((const struct directory_queue *)b)->de;
	return subtree_name_cmp(de1->pathname, de1->de_pathlen,
				de2->pathname, de2->de_pathlen);
}

static struct directory_entry *sort_directories(struct directory_entry *de,
						struct directory_queue *queue)
{
	int i, nsubtrees;

	nsubtrees = de->de_nsubtrees;
	for (i = 0; i < nsubtrees; i++) {
		struct directory_entry *new_de;
		de = de->next;
		new_de = xmalloc(directory_entry_size(de->de_pathlen));
		memcpy(new_de, de, directory_entry_size(de->de_pathlen));
		queue[i].de = new_de;
		if (de->de_nsubtrees) {
			queue[i].down = xcalloc(de->de_nsubtrees,
					sizeof(struct directory_queue));
			de = sort_directories(de,
					queue[i].down);
		}
	}
	qsort(queue, nsubtrees, sizeof(struct directory_queue),
			compare_cache_tree_elements);
	return de;
}

/*
 * This function modifys the directory argument that is given to it.
 * Don't use it if the directory entries are still needed after.
 */
static struct cache_tree *cache_tree_convert_v5(struct directory_entry *de)
{
	struct directory_queue *queue;

	if (!de->de_nentries)
		return NULL;
	queue = xcalloc(1, sizeof(struct directory_queue));
	queue[0].de = de;
	queue[0].down = xcalloc(de->de_nsubtrees, sizeof(struct directory_queue));

	sort_directories(de, queue[0].down);
	return convert_one(queue, 0);
}

static void resolve_undo_convert_v5(struct index_state *istate,
				    struct conflict_entry *conflict)
{
	int i;

	while (conflict) {
		struct string_list_item *lost;
		struct resolve_undo_info *ui;
		struct conflict_part *cp;

		if (conflict->entries &&
		    (conflict->entries->flags & CONFLICT_CONFLICTED) != 0) {
			conflict = conflict->next;
			continue;
		}
		if (!istate->resolve_undo) {
			istate->resolve_undo = xcalloc(1, sizeof(struct string_list));
			istate->resolve_undo->strdup_strings = 1;
		}

		lost = string_list_insert(istate->resolve_undo, conflict->name);
		if (!lost->util)
			lost->util = xcalloc(1, sizeof(*ui));
		ui = lost->util;

		cp = conflict->entries;
		for (i = 0; i < 3; i++)
			ui->mode[i] = 0;
		while (cp) {
			ui->mode[conflict_stage(cp) - 1] = cp->entry_mode;
			hashcpy(ui->sha1[conflict_stage(cp) - 1], cp->sha1);
			cp = cp->next;
		}
		conflict = conflict->next;
	}
}

static int read_entries(struct index_state *istate, struct directory_entry **de,
			unsigned int *entry_offset, void **mmap,
			unsigned long mmap_size, unsigned int *nr,
			unsigned int *foffsetblock, struct cache_entry **prev)
{
	struct cache_entry *head = NULL, *tail = NULL;
	struct conflict_entry *conflict_queue;
	struct cache_entry *ce;
	int i;

	conflict_queue = NULL;
	if (read_conflicts(&conflict_queue, *de, mmap, mmap_size) < 0)
		return -1;
	resolve_undo_convert_v5(istate, conflict_queue);
	for (i = 0; i < (*de)->de_nfiles; i++) {
		if (read_entry(&ce,
			       *de,
			       entry_offset,
			       mmap,
			       mmap_size,
			       foffsetblock) < 0)
			return -1;
		ce_queue_push(&head, &tail, ce);
		*foffsetblock += 4;

		/*
		 * Add the conflicted entries at the end of the index file
		 * to the in memory format
		 */
		if (conflict_queue &&
		    (conflict_queue->entries->flags & CONFLICT_CONFLICTED) != 0 &&
		    !cache_name_compare(conflict_queue->name, conflict_queue->namelen,
					ce->name, ce_namelen(ce))) {
			struct conflict_part *cp;
			cp = conflict_queue->entries;
			cp = cp->next;
			while (cp) {
				ce = convert_conflict_part(cp,
						conflict_queue->name,
						conflict_queue->namelen);
				ce_queue_push(&head, &tail, ce);
				conflict_part_head_remove(&cp);
			}
			conflict_entry_head_remove(&conflict_queue);
		}
	}

	*de = (*de)->next;

	while (head) {
		if (*de != NULL
		    && strcmp(head->name, (*de)->pathname) > 0) {
			read_entries(istate,
				     de,
				     entry_offset,
				     mmap,
				     mmap_size,
				     nr,
				     foffsetblock,
				     prev);
		} else {
			ce = ce_queue_pop(&head);
			set_index_entry(istate, *nr, ce);
			if (*prev)
				(*prev)->next_ce = ce;
			(*nr)++;
			*prev = ce;
			ce->next = NULL;
		}
	}
	return 0;
}

static struct directory_entry *read_head_directories(struct index_state *istate,
						     unsigned int *entry_offset,
						     unsigned int *foffsetblock,
						     unsigned int *ndirs,
						     void *mmap, unsigned long mmap_size)
{
	unsigned int dir_offset, dir_table_offset;
	struct cache_version_header *hdr;
	struct cache_header *hdr_v5;
	struct directory_entry *root_directory;

	hdr = mmap;
	hdr_v5 = ptr_add(mmap, sizeof(*hdr));
	istate->version = ntohl(hdr->hdr_version);
	istate->cache_alloc = alloc_nr(ntohl(hdr_v5->hdr_nfile));
	istate->cache = xcalloc(istate->cache_alloc, sizeof(struct cache_entry *));
	istate->initialized = 1;

	/* Skip size of the header + crc sum + size of offsets */
	dir_offset = sizeof(*hdr) + sizeof(*hdr_v5) + 4 + (ntohl(hdr_v5->hdr_ndir) + 1) * 4;
	dir_table_offset = sizeof(*hdr) + sizeof(*hdr_v5) + 4;
	root_directory = read_directories(&dir_offset, &dir_table_offset,
					  mmap, mmap_size);

	*entry_offset = ntohl(hdr_v5->hdr_fblockoffset);
	*foffsetblock = dir_offset;
	*ndirs = ntohl(hdr_v5->hdr_ndir);
	return root_directory;
}

static int read_index_filtered_v5(struct index_state *istate, void *mmap,
				  unsigned long mmap_size, struct filter_opts *opts)
{
	unsigned int entry_offset, ndirs, foffsetblock, nr = 0;
	struct directory_entry *root_directory, *de;
	int i, n;
	const char **adjusted_pathspec = NULL;
	int need_root = 1;
	char *seen, *oldpath;
	struct cache_entry *prev = NULL;

	root_directory = read_head_directories(istate, &entry_offset,
					       &foffsetblock, &ndirs,
					       mmap, mmap_size);

	if (opts && opts->pathspec) {
		need_root = 0;
		seen = xcalloc(1, ndirs);
		for (de = root_directory; de; de = de->next)
			match_pathspec(opts->pathspec, de->pathname, de->de_pathlen, 0, seen);
		for (n = 0; opts->pathspec[n]; n++)
			/* just count */;
		adjusted_pathspec = xmalloc((n+1)*sizeof(char *));
		adjusted_pathspec[n] = NULL;
		for (i = 0; i < n; i++) {
			if (seen[i] == MATCHED_EXACTLY)
				adjusted_pathspec[i] = opts->pathspec[i];
			else {
				char *super = strdup(opts->pathspec[i]);
				int len = strlen(super);
				while (len && super[len - 1] == '/')
					super[--len] = '\0'; /* strip trailing / */
				while (len && super[--len] != '/')
					; /* scan backwards to next / */
				if (len >= 0)
					super[len--] = '\0';
				if (len <= 0) {
					need_root = 1;
					break;
				}
				adjusted_pathspec[i] = super;
			}
		}
	}

	de = root_directory;
	while (de) {
		if (need_root ||
		    match_pathspec(adjusted_pathspec, de->pathname, de->de_pathlen, 0, NULL)) {
			unsigned int subdir_foffsetblock = de->de_foffset + foffsetblock;
			unsigned int *off = mmap + subdir_foffsetblock;
			unsigned int subdir_entry_offset = entry_offset + ntoh_l(*off);
			oldpath = de->pathname;
			do {
				if (read_entries(istate, &de, &subdir_entry_offset,
						 &mmap, mmap_size, &nr,
						 &subdir_foffsetblock, &prev) < 0)
					return -1;
			} while (de && !prefixcmp(de->pathname, oldpath));
		} else
			de = de->next;
	}
	istate->cache_tree = cache_tree_convert_v5(root_directory);
	istate->cache_nr = nr;
	istate->partially_read = 1;
	return 0;
}

static int read_index_v5(struct index_state *istate, void *mmap,
			 unsigned long mmap_size, struct filter_opts *opts)
{
	unsigned int entry_offset, ndirs, foffsetblock, nr = 0;
	struct directory_entry *root_directory, *de;
	struct cache_entry *prev = NULL;

	if (opts != NULL)
		return read_index_filtered_v5(istate, mmap, mmap_size, opts);

	root_directory = read_head_directories(istate, &entry_offset,
					       &foffsetblock, &ndirs,
					       mmap, mmap_size);
	de = root_directory;
	while (de)
		if (read_entries(istate, &de, &entry_offset, &mmap,
				 mmap_size, &nr, &foffsetblock, &prev) < 0)
			return -1;

	istate->cache_tree = cache_tree_convert_v5(root_directory);
	istate->cache_nr = nr;
	istate->partially_read = 0;
	return 0;
}

static void index_change_filter_opts_v5(struct index_state *istate, struct filter_opts *opts)
{
	if (istate->initialized == 1 &&
	    (((istate->filter_opts == NULL || opts == NULL) && istate->filter_opts != opts)
	     || (!memcmp(istate->filter_opts, opts, sizeof(*opts)))))
		return;
	discard_index(istate);
	read_index_filtered(istate, opts);
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
			int *total_dir_len,
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

static struct directory_entry *compile_directory_data(struct index_state *istate,
						int nfile,
						unsigned int *ndir,
						int *non_conflicted,
						int *total_dir_len,
						int *total_file_len)
{
	int i, dir_len = -1;
	char *dir;
	struct directory_entry *de, *current, *search, *found, *new, *previous_entry;
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
		int new_entry;
		if (cache[i]->ce_flags & CE_REMOVE)
			continue;

		new_entry = !ce_stage(cache[i]) || !conflict_entry
		    || cache_name_compare(conflict_entry->name, conflict_entry->namelen,
					cache[i]->name, ce_namelen(cache[i]));
		if (new_entry)
			(*non_conflicted)++;
		if (dir_len < 0 || strncmp(cache[i]->name, dir, dir_len)
		    || cache[i]->name[dir_len] != '/'
		    || strchr(cache[i]->name + dir_len + 1, '/')) {
			dir = super_directory(cache[i]->name);
			if (!dir)
				dir_len = 0;
			else
				dir_len = strlen(dir);
			crc = crc32(0, (Bytef*)dir, dir_len);
			found = lookup_hash(crc, &table);
			search = found;
			while (search && dir_len != 0 && strcmp(dir, search->pathname) != 0)
				search = search->next_hash;
		}
		previous_entry = current;
		if (!search || !found) {
			new = init_directory_entry(dir, dir_len);
			current->next = new;
			current = current->next;
			insert_directory_entry(new, &table, total_dir_len, ndir, crc);
			search = current;
		}
		if (new_entry) {
			search->de_nfiles++;
			*total_file_len += ce_namelen(cache[i]) + 1;
			if (search->de_pathlen)
				*total_file_len -= search->de_pathlen + 1;
			ce_queue_push(&(search->ce), &(search->ce_last), cache[i]);
		}
		if (ce_stage(cache[i]) > 0) {
			struct conflict_part *conflict_part;
			if (new_entry) {
				conflict_entry = create_conflict_entry_from_ce(cache[i], search->de_pathlen);
				add_conflict_to_directory_entry(search, conflict_entry);
			}
			conflict_part = conflict_part_from_inmemory(cache[i]);
			add_part_to_conflict_entry(search, conflict_entry, conflict_part);
		}
		if (dir && !found) {
			struct directory_entry *no_subtrees;

			no_subtrees = current;
			dir = super_directory(dir);
			if (dir)
				dir_len = strlen(dir);
			else
				dir_len = 0;
			crc = crc32(0, (Bytef*)dir, dir_len);
			found = lookup_hash(crc, &table);
			while (!found) {
				new = init_directory_entry(dir, dir_len);
				new->de_nsubtrees = 1;
				new->next = no_subtrees;
				no_subtrees = new;
				insert_directory_entry(new, &table, total_dir_len, ndir, crc);
				dir = super_directory(dir);
				if (!dir)
					dir_len = 0;
				else
					dir_len = strlen(dir);
				crc = crc32(0, (Bytef*)dir, dir_len);
				found = lookup_hash(crc, &table);
			}
			search = found;
			while (search->next_hash && strcmp(dir, search->pathname) != 0)
				search = search->next_hash;
			if (search)
				found = search;
			found->de_nsubtrees++;
			previous_entry->next = no_subtrees;
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
	 * This is needed because the compiler aligns structs to sizes multipe
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
			ce = ce->next;
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
			ce = ce->next;
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
	struct cache_version_header hdr;
	struct cache_header hdr_v5;
	struct cache_entry **cache = istate->cache;
	struct directory_entry *de;
	struct ondisk_directory_entry *ondisk;
	int entries = istate->cache_nr;
	int i, removed, non_conflicted, total_dir_len, ondisk_directory_size;
	int total_file_len, conflict_offset, offset_to_offset;
	unsigned int ndir;
	uint32_t crc;

	if (istate->partially_read)
		die("BUG: index: cannot write a partially read index");

	for (i = removed = 0; i < entries; i++) {
		if (cache[i]->ce_flags & CE_REMOVE)
			removed++;
	}
	hdr.hdr_signature = htonl(CACHE_SIGNATURE);
	hdr.hdr_version = htonl(istate->version);
	hdr_v5.hdr_nfile = htonl(entries - removed);
	hdr_v5.hdr_nextension = htonl(0); /* Currently no extensions are supported */

	non_conflicted = 0;
	total_dir_len = 0;
	total_file_len = 0;
	de = compile_directory_data(istate, entries, &ndir, &non_conflicted,
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
	hdr_v5.hdr_fblockoffset = htonl(sizeof(hdr) + sizeof(hdr_v5) + 4
		+ (ndir + 1) * 4
		+ total_dir_len
		+ ndir * (ondisk_directory_size + 4)
		+ (non_conflicted + 1) * 4);

	crc = 0;
	if (ce_write(&crc, newfd, &hdr, sizeof(hdr)) < 0)
		return -1;
	if (ce_write(&crc, newfd, &hdr_v5, sizeof(hdr_v5)) < 0)
		return -1;
	crc = htonl(crc);
	if (ce_write(NULL, newfd, &crc, 4) < 0)
		return -1;

	conflict_offset = sizeof(hdr) + sizeof(hdr_v5) + 4
		+ (ndir + 1) * 4
		+ total_dir_len
		+ ndir * (ondisk_directory_size + 4)
		+ (non_conflicted + 1) * 4
		+ total_file_len
		+ non_conflicted * (sizeof(struct ondisk_cache_entry) + 4);
	if (write_directories(de, newfd, conflict_offset) < 0)
		return -1;
	offset_to_offset = sizeof(hdr) + sizeof(hdr_v5) + 4
		+ (ndir + 1) * 4
		+ total_dir_len
		+ ndir * (ondisk_directory_size + 4);
	if (write_entries(istate, de, entries, newfd, offset_to_offset) < 0)
		return -1;
	if (write_conflicts(istate, de, newfd) < 0)
		return -1;
	return ce_flush(newfd);
}

struct index_ops v5_ops = {
	match_stat_basic,
	verify_hdr,
	read_index_v5,
	write_index_v5,
	index_change_filter_opts_v5
};
