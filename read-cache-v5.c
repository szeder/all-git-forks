#include "cache.h"
#include "read-cache.h"
#include "resolve-undo.h"
#include "cache-tree.h"

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
	ctimens = ST_MTIME_NSEC(*st);
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

static int match_stat_basic(struct cache_entry *ce,
				struct stat *st,
				int changed)
{

	if (ce->ce_mtime.sec != 0 && ce->ce_mtime.sec != (unsigned int)st->st_mtime)
		changed |= MTIME_CHANGED;
#ifdef USE_NSEC
	if (ce->ce_mtime.nsec != 0 && ce->ce_mtime.nsec != ST_MTIME_NSEC(*st))
		changed |= MTIME_CHANGED;
#endif
	if (ce->ce_size != (unsigned int)st->st_size)
		changed |= DATA_CHANGED;

	if (ce->ce_stat_crc != 0 && !match_stat_crc(st, ce->ce_stat_crc)) {
		changed |= OWNER_CHANGED;
		changed |= INODE_CHANGED;
	}
	/* Racily smudged entry? */
	if (!ce->ce_mtime.sec && !ce->ce_mtime.nsec) {
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
	hdr_v5 = (struct cache_header *)((char *)mmap + sizeof(*hdr));
	/* Size of the header + the size of the extensionoffsets */
	header_size = sizeof(*hdr) + sizeof(*hdr_v5) + hdr_v5->hdr_nextension * 4;
	/* Initialize crc */
	filecrc = (uint32_t *)((char *)mmap + header_size);
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
	ce->ce_ctime.sec  = 0;
	ce->ce_mtime.sec  = ntoh_l(ondisk->mtime.sec);
	ce->ce_ctime.nsec = 0;
	ce->ce_mtime.nsec = ntoh_l(ondisk->mtime.nsec);
	ce->ce_dev        = 0;
	ce->ce_ino        = 0;
	ce->ce_mode       = ntoh_s(ondisk->mode);
	ce->ce_uid        = 0;
	ce->ce_gid        = 0;
	ce->ce_size       = ntoh_l(ondisk->size);
	ce->ce_flags      = flags & CE_STAGEMASK;
	ce->ce_flags     |= flags & CE_VALID;
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

	ce->ce_ctime.sec  = 0;
	ce->ce_mtime.sec  = 0;
	ce->ce_ctime.nsec = 0;
	ce->ce_mtime.nsec = 0;
	ce->ce_dev        = 0;
	ce->ce_ino        = 0;
	ce->ce_mode       = cp->entry_mode;
	ce->ce_uid        = 0;
	ce->ce_gid        = 0;
	ce->ce_size       = 0;
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

	ondisk_directory_size = sizeof(disk_de->flags)
		+ sizeof(disk_de->foffset)
		+ sizeof(disk_de->cr)
		+ sizeof(disk_de->ncr)
		+ sizeof(disk_de->nsubtrees)
		+ sizeof(disk_de->nfiles)
		+ sizeof(disk_de->nentries)
		+ sizeof(disk_de->sha1);
	name = (char *)mmap + *dir_offset;
	beginning = (uint32_t *)((char *)mmap + *dir_table_offset);
	end = (uint32_t *)((char *)mmap + *dir_table_offset + 4);
	len = ntoh_l(*end) - ntoh_l(*beginning) - ondisk_directory_size - 5;
	disk_de = (struct ondisk_directory_entry *)
			((char *)mmap + *dir_offset + len + 1);
	de = directory_entry_from_ondisk(disk_de, name, len);
	de->next = NULL;

	/* Length of pathname + nul byte for termination + size of
	 * members of ondisk_directory_entry. (Just using the size
	 * of the stuct doesn't work, because there may be padding
	 * bytes for the struct)
	 */
	data_len = len + 1 + ondisk_directory_size;

	filecrc = (uint32_t *)((char *)mmap + *dir_offset + data_len);
	if (!check_crc32(0, mmap + *dir_offset, data_len, ntoh_l(*filecrc)))
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

static struct cache_entry *read_entry(struct directory_entry *de,
			unsigned long *entry_offset,
			void **mmap,
			unsigned long mmap_size,
			unsigned int *foffsetblock,
			int fd)
{
	int len, crc_wrong, i = 0, offset_to_offset;
	char *name;
	uint32_t foffsetblockcrc;
	uint32_t *filecrc, *beginning, *end;
	struct cache_entry *ce;
	struct ondisk_cache_entry *disk_ce;

	do {
		name = (char *)*mmap + *entry_offset;
		beginning = (uint32_t *)((char *)*mmap + *foffsetblock);
		end = (uint32_t *)((char *)*mmap + *foffsetblock + 4);
		len = ntoh_l(*end) - ntoh_l(*beginning) - sizeof(struct ondisk_cache_entry) - 5;
		disk_ce = (struct ondisk_cache_entry *)
				((char *)*mmap + *entry_offset + len + 1);
		ce = cache_entry_from_ondisk(disk_ce, de, name, len, de->de_pathlen);
		filecrc = (uint32_t *)((char *)*mmap + *entry_offset + len + 1 + sizeof(*disk_ce));
		offset_to_offset = htonl(*foffsetblock);
		foffsetblockcrc = crc32(0, (Bytef*)&offset_to_offset, 4);
		crc_wrong = !check_crc32(foffsetblockcrc,
			(char *)*mmap + *entry_offset, len + 1 + sizeof(*disk_ce),
			ntoh_l(*filecrc));
		if (crc_wrong) {
			/* wait for 10 milliseconds */
			usleep(10*1000);
			munmap(*mmap, mmap_size);
			*mmap = xmmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
		}
		i++;
		/*
		 * Retry for 500 ms maximum, before giving up and saying the
		 * checksum is wrong.
		 */
	} while (crc_wrong && i < 50);
	if (crc_wrong)
		goto unmap;
	*entry_offset += len + 1 + sizeof(*disk_ce) + 4;
	return ce;
unmap:
	munmap(*mmap, mmap_size);
	die("file crc doesn't match for '%s'", ce->name);
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

static struct conflict_entry *read_conflicts(struct directory_entry *de,
						void **mmap,
						unsigned long mmap_size,
						int fd)
{
	struct conflict_entry *head, *tail;
	unsigned int croffset, i, j = 0;
	char *full_name;

	croffset = de->de_cr;
	tail = NULL;
	head = NULL;
	for (i = 0; i < de->de_ncr; i++) {
		struct conflict_entry *conflict_new;
		unsigned int len, *nfileconflicts;
		char *name;
		void *crc_start;
		int k, offset, crc_wrong;
		uint32_t *filecrc;

		do {
			offset = croffset;
			crc_start = (void *)((char *)*mmap + offset);
			name = (char *)*mmap + offset;
			len = strlen(name);
			offset += len + 1;
			nfileconflicts = (unsigned int *)((char *)*mmap + offset);
			offset += 4;

			full_name = xmalloc(sizeof(char) * (len + de->de_pathlen));
			memcpy(full_name, de->pathname, de->de_pathlen);
			memcpy(full_name + de->de_pathlen, name, len);
			conflict_new = create_new_conflict(full_name,
					len + de->de_pathlen, de->de_pathlen);
			for (k = 0; k < ntoh_l(*nfileconflicts); k++) {
				struct ondisk_conflict_part *ondisk;
				struct conflict_part *cp;

				ondisk = (struct ondisk_conflict_part *)((char *)*mmap + offset);
				cp = conflict_part_from_ondisk(ondisk);
				cp->next = NULL;
				add_part_to_conflict_entry(de, conflict_new, cp);
				offset += sizeof(struct ondisk_conflict_part);
			}
			filecrc = (uint32_t *)((char *)*mmap + offset);
			crc_wrong = !check_crc32(0, crc_start,
				len + 1 + 4 + conflict_new->nfileconflicts
				* sizeof(struct ondisk_conflict_part),
				ntoh_l(*filecrc));
			if (crc_wrong) {
				/* wait for 10 milliseconds */
				usleep(10*1000);
				munmap(*mmap, mmap_size);
				*mmap = xmmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
			}
			free(full_name);
			j++;
		} while (crc_wrong && j < 50);
		if (crc_wrong)
			goto unmap;
		croffset = offset + 4;
		conflict_entry_push(&head, &tail, conflict_new);
	}
	return head;
unmap:
	munmap(*mmap, mmap_size);
	die("wrong crc for conflict: %s", full_name);
}

static struct directory_entry *read_entries(struct index_state *istate,
					struct directory_entry *de,
					unsigned long *entry_offset,
					void **mmap,
					unsigned long mmap_size,
					int *nr,
					unsigned int *foffsetblock,
					int fd)
{
	struct cache_entry *head = NULL, *tail = NULL;
	struct conflict_entry *conflict_queue;
	struct cache_entry *ce;
	int i;

	conflict_queue = read_conflicts(de, mmap, mmap_size, fd);
	resolve_undo_convert_v5(istate, conflict_queue);
	for (i = 0; i < de->de_nfiles; i++) {
		ce = read_entry(de,
				entry_offset,
				mmap,
				mmap_size,
				foffsetblock,
				fd);
		ce_queue_push(&head, &tail, ce);
		*foffsetblock += 4;

		/* Add the conflicted entries at the end of the index file
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

	de = de->next;

	while (head) {
		if (de != NULL
		    && strcmp(head->name, de->pathname) > 0) {
			de = read_entries(istate,
					de,
					entry_offset,
					mmap,
					mmap_size,
					nr,
					foffsetblock,
					fd);
		} else {
			ce = ce_queue_pop(&head);
			set_index_entry(istate, *nr, ce);
			(*nr)++;
		}
	}

	return de;
}

static void read_index_v5(struct index_state *istate, void *mmap, int mmap_size, int fd)
{
	unsigned long entry_offset;
	unsigned int dir_offset, dir_table_offset;
	struct cache_version_header *hdr;
	struct cache_header *hdr_v5;
	struct directory_entry *root_directory, *de;
	int nr;
	unsigned int foffsetblock;

	hdr = mmap;
	hdr_v5 = (struct cache_header *)((char *)mmap + sizeof(*hdr));
	istate->version = ntohl(hdr->hdr_version);
	istate->cache_nr = ntohl(hdr_v5->hdr_nfile);
	istate->cache_alloc = alloc_nr(istate->cache_nr);
	istate->cache = xcalloc(istate->cache_alloc, sizeof(struct cache_entry *));
	istate->initialized = 1;

	/* Skip size of the header + crc sum + size of offsets */
	dir_offset = sizeof(*hdr) + sizeof(*hdr_v5) + 4 + (ntohl(hdr_v5->hdr_ndir) + 1) * 4;
	dir_table_offset = sizeof(*hdr) + sizeof(*hdr_v5) + 4;
	root_directory = read_directories(&dir_offset, &dir_table_offset, mmap, mmap_size);

	entry_offset = ntohl(hdr_v5->hdr_fblockoffset);

	nr = 0;
	foffsetblock = dir_offset;
	de = root_directory;
	while (de)
		de = read_entries(istate, de, &entry_offset,
				&mmap, mmap_size, &nr, &foffsetblock, fd);
	istate->cache_tree = cache_tree_convert_v5(root_directory);
}

struct index_ops v5_ops = {
	match_stat_basic,
	verify_hdr,
	read_index_v5,
	NULL
};
