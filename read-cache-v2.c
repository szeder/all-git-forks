#include "cache.h"
#include "read-cache.h"
#include "resolve-undo.h"
#include "cache-tree.h"
#include "varint.h"

/* Mask for the name length in ce_flags in the on-disk index */
#define CE_NAMEMASK  (0x0fff)

struct cache_header {
	unsigned int hdr_entries;
};

/*****************************************************************
 * Index File I/O
 *****************************************************************/

/*
 * dev/ino/uid/gid/size are also just tracked to the low 32 bits
 * Again - this is just a (very strong in practice) heuristic that
 * the inode hasn't changed.
 *
 * We save the fields in big-endian order to allow using the
 * index file over NFS transparently.
 */
struct ondisk_cache_entry {
	struct cache_time ctime;
	struct cache_time mtime;
	unsigned int dev;
	unsigned int ino;
	unsigned int mode;
	unsigned int uid;
	unsigned int gid;
	unsigned int size;
	unsigned char sha1[20];
	unsigned short flags;
	char name[FLEX_ARRAY]; /* more */
};

/*
 * This struct is used when CE_EXTENDED bit is 1
 * The struct must match ondisk_cache_entry exactly from
 * ctime till flags
 */
struct ondisk_cache_entry_extended {
	struct cache_time ctime;
	struct cache_time mtime;
	unsigned int dev;
	unsigned int ino;
	unsigned int mode;
	unsigned int uid;
	unsigned int gid;
	unsigned int size;
	unsigned char sha1[20];
	unsigned short flags;
	unsigned short flags2;
	char name[FLEX_ARRAY]; /* more */
};

/* These are only used for v3 or lower */
#define align_flex_name(STRUCT,len) ((offsetof(struct STRUCT,name) + (len) + 8) & ~7)
#define ondisk_cache_entry_size(len) align_flex_name(ondisk_cache_entry,len)
#define ondisk_cache_entry_extended_size(len) align_flex_name(ondisk_cache_entry_extended,len)
#define ondisk_ce_size(ce) (((ce)->ce_flags & CE_EXTENDED) ? \
			    ondisk_cache_entry_extended_size(ce_namelen(ce)) : \
			    ondisk_cache_entry_size(ce_namelen(ce)))

static int verify_hdr(void *mmap, unsigned long size)
{
	git_SHA_CTX c;
	unsigned char sha1[20];

	if (size < sizeof(struct cache_version_header)
			+ sizeof(struct cache_header) + 20)
		die("index file smaller than expected");

	git_SHA1_Init(&c);
	git_SHA1_Update(&c, mmap, size - 20);
	git_SHA1_Final(sha1, &c);
	if (hashcmp(sha1, (unsigned char *)mmap + size - 20))
		return error("bad index file sha1 signature");
	return 0;
}

static int match_stat_basic(const struct cache_entry *ce,
			    struct stat *st, int changed)
{
	changed |= match_stat_data(&ce->ce_stat_data, st);

	/* Racily smudged entry? */
	if (!ce->ce_stat_data.sd_size) {
		if (!is_empty_blob_sha1(ce->sha1))
			changed |= DATA_CHANGED;
	}
	return changed;
}

static struct cache_entry *cache_entry_from_ondisk(struct ondisk_cache_entry *ondisk,
						   unsigned int flags,
						   const char *name,
						   size_t len)
{
	struct cache_entry *ce = xmalloc(cache_entry_size(len));

	ce->ce_stat_data.sd_ctime.sec = ntoh_l(ondisk->ctime.sec);
	ce->ce_stat_data.sd_mtime.sec = ntoh_l(ondisk->mtime.sec);
	ce->ce_stat_data.sd_ctime.nsec = ntoh_l(ondisk->ctime.nsec);
	ce->ce_stat_data.sd_mtime.nsec = ntoh_l(ondisk->mtime.nsec);
	ce->ce_stat_data.sd_dev   = ntoh_l(ondisk->dev);
	ce->ce_stat_data.sd_ino   = ntoh_l(ondisk->ino);
	ce->ce_mode  = ntoh_l(ondisk->mode);
	ce->ce_stat_data.sd_uid   = ntoh_l(ondisk->uid);
	ce->ce_stat_data.sd_gid   = ntoh_l(ondisk->gid);
	ce->ce_stat_data.sd_size  = ntoh_l(ondisk->size);
	ce->ce_flags = flags & ~CE_NAMEMASK;
	ce->ce_namelen = len;
	hashcpy(ce->sha1, ondisk->sha1);
	memcpy(ce->name, name, len);
	ce->name[len] = '\0';
	return ce;
}

/*
 * Adjacent cache entries tend to share the leading paths, so it makes
 * sense to only store the differences in later entries.  In the v4
 * on-disk format of the index, each on-disk cache entry stores the
 * number of bytes to be stripped from the end of the previous name,
 * and the bytes to append to the result, to come up with its name.
 */
static unsigned long expand_name_field(struct strbuf *name, const char *cp_)
{
	const unsigned char *ep, *cp = (const unsigned char *)cp_;
	size_t len = decode_varint(&cp);

	if (name->len < len)
		die("malformed name field in the index");
	strbuf_remove(name, name->len - len, len);
	for (ep = cp; *ep; ep++)
		; /* find the end */
	strbuf_add(name, cp, ep - cp);
	return (const char *)ep + 1 - cp_;
}

static struct cache_entry *create_from_disk(struct ondisk_cache_entry *ondisk,
					    unsigned long *ent_size,
					    struct strbuf *previous_name)
{
	struct cache_entry *ce;
	size_t len;
	const char *name;
	unsigned int flags;

	/* On-disk flags are just 16 bits */
	flags = ntoh_s(ondisk->flags);
	len = flags & CE_NAMEMASK;

	if (flags & CE_EXTENDED) {
		struct ondisk_cache_entry_extended *ondisk2;
		int extended_flags;
		ondisk2 = (struct ondisk_cache_entry_extended *)ondisk;
		extended_flags = ntoh_s(ondisk2->flags2) << 16;
		/* We do not yet understand any bit out of CE_EXTENDED_FLAGS */
		if (extended_flags & ~CE_EXTENDED_FLAGS)
			die("Unknown index entry format %08x", extended_flags);
		flags |= extended_flags;
		name = ondisk2->name;
	}
	else
		name = ondisk->name;

	if (!previous_name) {
		/* v3 and earlier */
		if (len == CE_NAMEMASK)
			len = strlen(name);
		ce = cache_entry_from_ondisk(ondisk, flags, name, len);

		*ent_size = ondisk_ce_size(ce);
	} else {
		unsigned long consumed;
		consumed = expand_name_field(previous_name, name);
		ce = cache_entry_from_ondisk(ondisk, flags,
					     previous_name->buf,
					     previous_name->len);

		*ent_size = (name - ((char *)ondisk)) + consumed;
	}
	return ce;
}

static int read_index_extension(struct index_state *istate,
				const char *ext, void *data, unsigned long sz)
{
	switch (CACHE_EXT(ext)) {
	case CACHE_EXT_TREE:
		istate->cache_tree = cache_tree_read(data, sz);
		break;
	case CACHE_EXT_RESOLVE_UNDO:
		istate->resolve_undo = resolve_undo_read(data, sz);
		break;
	default:
		if (*ext < 'A' || 'Z' < *ext)
			return error("index uses %.4s extension, which we do not understand",
				     ext);
		fprintf(stderr, "ignoring %.4s extension\n", ext);
		break;
	}
	return 0;
}

static int read_index_v2(struct index_state *istate, void *mmap,
			 unsigned long mmap_size)
{
	int i;
	unsigned long src_offset;
	struct cache_version_header *hdr;
	struct cache_header *hdr_v2;
	struct strbuf previous_name_buf = STRBUF_INIT, *previous_name;

	hdr = mmap;
	hdr_v2 = (struct cache_header *)((char *)mmap + sizeof(*hdr));
	istate->version = ntohl(hdr->hdr_version);
	istate->cache_nr = ntohl(hdr_v2->hdr_entries);
	istate->cache_alloc = alloc_nr(istate->cache_nr);
	istate->cache = xcalloc(istate->cache_alloc, sizeof(struct cache_entry *));
	istate->initialized = 1;

	if (istate->version == 4)
		previous_name = &previous_name_buf;
	else
		previous_name = NULL;

	src_offset = sizeof(*hdr) + sizeof(*hdr_v2);
	for (i = 0; i < istate->cache_nr; i++) {
		struct ondisk_cache_entry *disk_ce;
		struct cache_entry *ce;
		unsigned long consumed;

		disk_ce = (struct ondisk_cache_entry *)((char *)mmap + src_offset);
		ce = create_from_disk(disk_ce, &consumed, previous_name);
		set_index_entry(istate, i, ce);

		src_offset += consumed;
	}
	strbuf_release(&previous_name_buf);

	while (src_offset <= mmap_size - 20 - 8) {
		/* After an array of active_nr index entries,
		 * there can be arbitrary number of extended
		 * sections, each of which is prefixed with
		 * extension name (4-byte) and section length
		 * in 4-byte network byte order.
		 */
		uint32_t extsize;
		memcpy(&extsize, (char *)mmap + src_offset + 4, 4);
		extsize = ntohl(extsize);
		if (read_index_extension(istate,
					(const char *) mmap + src_offset,
					(char *) mmap + src_offset + 8,
					extsize) < 0)
			goto unmap;
		src_offset += 8;
		src_offset += extsize;
	}
	return 0;
unmap:
	munmap(mmap, mmap_size);
	die("index file corrupt");
}

#define WRITE_BUFFER_SIZE 8192
static unsigned char write_buffer[WRITE_BUFFER_SIZE];
static unsigned long write_buffer_len;

static int ce_write_flush(git_SHA_CTX *context, int fd)
{
	unsigned int buffered = write_buffer_len;
	if (buffered) {
		git_SHA1_Update(context, write_buffer, buffered);
		if (write_in_full(fd, write_buffer, buffered) != buffered)
			return -1;
		write_buffer_len = 0;
	}
	return 0;
}

static int ce_write(git_SHA_CTX *context, int fd, void *data, unsigned int len)
{
	while (len) {
		unsigned int buffered = write_buffer_len;
		unsigned int partial = WRITE_BUFFER_SIZE - buffered;
		if (partial > len)
			partial = len;
		memcpy(write_buffer + buffered, data, partial);
		buffered += partial;
		if (buffered == WRITE_BUFFER_SIZE) {
			write_buffer_len = buffered;
			if (ce_write_flush(context, fd))
				return -1;
			buffered = 0;
		}
		write_buffer_len = buffered;
		len -= partial;
		data = (char *) data + partial;
	}
	return 0;
}

static int write_index_ext_header(git_SHA_CTX *context, int fd,
				  unsigned int ext, unsigned int sz)
{
	ext = htonl(ext);
	sz = htonl(sz);
	return ((ce_write(context, fd, &ext, 4) < 0) ||
		(ce_write(context, fd, &sz, 4) < 0)) ? -1 : 0;
}

static int ce_flush(git_SHA_CTX *context, int fd)
{
	unsigned int left = write_buffer_len;

	if (left) {
		write_buffer_len = 0;
		git_SHA1_Update(context, write_buffer, left);
	}

	/* Flush first if not enough space for SHA1 signature */
	if (left + 20 > WRITE_BUFFER_SIZE) {
		if (write_in_full(fd, write_buffer, left) != left)
			return -1;
		left = 0;
	}

	/* Append the SHA1 signature at the end */
	git_SHA1_Final(write_buffer + left, context);
	left += 20;
	return (write_in_full(fd, write_buffer, left) != left) ? -1 : 0;
}

static void ce_smudge_racily_clean_entry(struct index_state *istate, struct cache_entry *ce)
{
	/*
	 * The only thing we care about in this function is to smudge the
	 * falsely clean entry due to touch-update-touch race, so we leave
	 * everything else as they are.  We are called for entries whose
	 * ce_stat_data.sd_mtime match the index file mtime.
	 *
	 * Note that this actually does not do much for gitlinks, for
	 * which ce_match_stat_basic() always goes to the actual
	 * contents.  The caller checks with is_racy_timestamp() which
	 * always says "no" for gitlinks, so we are not called for them ;-)
	 */
	struct stat st;

	if (lstat(ce->name, &st) < 0)
		return;
	if (ce_match_stat_basic(istate, ce, &st))
		return;
	if (ce_modified_check_fs(ce, &st)) {
		/* This is "racily clean"; smudge it.  Note that this
		 * is a tricky code.  At first glance, it may appear
		 * that it can break with this sequence:
		 *
		 * $ echo xyzzy >frotz
		 * $ git-update-index --add frotz
		 * $ : >frotz
		 * $ sleep 3
		 * $ echo filfre >nitfol
		 * $ git-update-index --add nitfol
		 *
		 * but it does not.  When the second update-index runs,
		 * it notices that the entry "frotz" has the same timestamp
		 * as index, and if we were to smudge it by resetting its
		 * size to zero here, then the object name recorded
		 * in index is the 6-byte file but the cached stat information
		 * becomes zero --- which would then match what we would
		 * obtain from the filesystem next time we stat("frotz").
		 *
		 * However, the second update-index, before calling
		 * this function, notices that the cached size is 6
		 * bytes and what is on the filesystem is an empty
		 * file, and never calls us, so the cached size information
		 * for "frotz" stays 6 which does not match the filesystem.
		 */
		ce->ce_stat_data.sd_size = 0;
	}
}

/* Copy miscellaneous fields but not the name */
static char *copy_cache_entry_to_ondisk(struct ondisk_cache_entry *ondisk,
				       struct cache_entry *ce)
{
	short flags;

	ondisk->ctime.sec = htonl(ce->ce_stat_data.sd_ctime.sec);
	ondisk->mtime.sec = htonl(ce->ce_stat_data.sd_mtime.sec);
	ondisk->ctime.nsec = htonl(ce->ce_stat_data.sd_ctime.nsec);
	ondisk->mtime.nsec = htonl(ce->ce_stat_data.sd_mtime.nsec);
	ondisk->dev  = htonl(ce->ce_stat_data.sd_dev);
	ondisk->ino  = htonl(ce->ce_stat_data.sd_ino);
	ondisk->mode = htonl(ce->ce_mode);
	ondisk->uid  = htonl(ce->ce_stat_data.sd_uid);
	ondisk->gid  = htonl(ce->ce_stat_data.sd_gid);
	ondisk->size = htonl(ce->ce_stat_data.sd_size);
	hashcpy(ondisk->sha1, ce->sha1);

	flags = ce->ce_flags;
	flags |= (ce_namelen(ce) >= CE_NAMEMASK ? CE_NAMEMASK : ce_namelen(ce));
	ondisk->flags = htons(flags);
	if (ce->ce_flags & CE_EXTENDED) {
		struct ondisk_cache_entry_extended *ondisk2;
		ondisk2 = (struct ondisk_cache_entry_extended *)ondisk;
		ondisk2->flags2 = htons((ce->ce_flags & CE_EXTENDED_FLAGS) >> 16);
		return ondisk2->name;
	}
	else {
		return ondisk->name;
	}
}

static int ce_write_entry(git_SHA_CTX *c, int fd, struct cache_entry *ce,
			  struct strbuf *previous_name)
{
	int size;
	struct ondisk_cache_entry *ondisk;
	char *name;
	int result;

	if (!previous_name) {
		size = ondisk_ce_size(ce);
		ondisk = xcalloc(1, size);
		name = copy_cache_entry_to_ondisk(ondisk, ce);
		memcpy(name, ce->name, ce_namelen(ce));
	} else {
		int common, to_remove, prefix_size;
		unsigned char to_remove_vi[16];
		for (common = 0;
		     (ce->name[common] &&
		      common < previous_name->len &&
		      ce->name[common] == previous_name->buf[common]);
		     common++)
			; /* still matching */
		to_remove = previous_name->len - common;
		prefix_size = encode_varint(to_remove, to_remove_vi);

		if (ce->ce_flags & CE_EXTENDED)
			size = offsetof(struct ondisk_cache_entry_extended, name);
		else
			size = offsetof(struct ondisk_cache_entry, name);
		size += prefix_size + (ce_namelen(ce) - common + 1);

		ondisk = xcalloc(1, size);
		name = copy_cache_entry_to_ondisk(ondisk, ce);
		memcpy(name, to_remove_vi, prefix_size);
		memcpy(name + prefix_size, ce->name + common, ce_namelen(ce) - common);

		strbuf_splice(previous_name, common, to_remove,
			      ce->name + common, ce_namelen(ce) - common);
	}

	result = ce_write(c, fd, ondisk, size);
	free(ondisk);
	return result;
}

static int write_index_v2(struct index_state *istate, int newfd)
{
	git_SHA_CTX c;
	struct cache_version_header hdr;
	struct cache_header hdr_v2;
	int i, err, removed, extended, hdr_version;
	struct cache_entry **cache = istate->cache;
	int entries = istate->cache_nr;
	struct stat st;
	struct strbuf previous_name_buf = STRBUF_INIT, *previous_name;

	for (i = removed = extended = 0; i < entries; i++) {
		if (cache[i]->ce_flags & CE_REMOVE)
			removed++;

		/* reduce extended entries if possible */
		cache[i]->ce_flags &= ~CE_EXTENDED;
		if (cache[i]->ce_flags & CE_EXTENDED_FLAGS) {
			extended++;
			cache[i]->ce_flags |= CE_EXTENDED;
		}
	}

	if (!istate->version)
		istate->version = INDEX_FORMAT_DEFAULT;

	/* demote version 3 to version 2 when the latter suffices */
	if (istate->version == 3 || istate->version == 2)
		istate->version = extended ? 3 : 2;

	hdr_version = istate->version;

	hdr.hdr_signature = htonl(CACHE_SIGNATURE);
	hdr.hdr_version = htonl(hdr_version);
	hdr_v2.hdr_entries = htonl(entries - removed);

	git_SHA1_Init(&c);
	if (ce_write(&c, newfd, &hdr, sizeof(hdr)) < 0)
		return -1;
	if (ce_write(&c, newfd, &hdr_v2, sizeof(hdr_v2)) < 0)
		return -1;

	previous_name = (hdr_version == 4) ? &previous_name_buf : NULL;
	for (i = 0; i < entries; i++) {
		struct cache_entry *ce = cache[i];
		if (ce->ce_flags & CE_REMOVE)
			continue;
		if (!ce_uptodate(ce) && is_racy_timestamp(istate, ce))
			ce_smudge_racily_clean_entry(istate, ce);
		if (is_null_sha1(ce->sha1))
			return error("cache entry has null sha1: %s", ce->name);
		if (ce_write_entry(&c, newfd, ce, previous_name) < 0)
			return -1;
	}
	strbuf_release(&previous_name_buf);

	/* Write extension data here */
	if (istate->cache_tree) {
		struct strbuf sb = STRBUF_INIT;

		cache_tree_write(&sb, istate->cache_tree);
		err = write_index_ext_header(&c, newfd, CACHE_EXT_TREE, sb.len) < 0
			|| ce_write(&c, newfd, sb.buf, sb.len) < 0;
		strbuf_release(&sb);
		if (err)
			return -1;
	}
	if (istate->resolve_undo) {
		struct strbuf sb = STRBUF_INIT;

		resolve_undo_write(&sb, istate->resolve_undo);
		err = write_index_ext_header(&c, newfd, CACHE_EXT_RESOLVE_UNDO,
					     sb.len) < 0
			|| ce_write(&c, newfd, sb.buf, sb.len) < 0;
		strbuf_release(&sb);
		if (err)
			return -1;
	}

	if (ce_flush(&c, newfd) || fstat(newfd, &st))
		return -1;
	istate->timestamp.sec = (unsigned int)st.st_mtime;
	istate->timestamp.nsec = ST_MTIME_NSEC(st);
	return 0;
}

struct index_ops v2_ops = {
	match_stat_basic,
	verify_hdr,
	read_index_v2,
	write_index_v2
};
