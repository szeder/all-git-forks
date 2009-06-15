#include "cache.h"
#include "object.h"
#include "commit.h"
#include "tree.h"
#include "tree-walk.h"
#include "blob.h"
#include "tag.h"
#include "diff.h"
#include "revision.h"
#include "run-command.h"


/* single index maps objects to cache files */
struct index_header {
	char signature[8]; /* REVINDEX */
	unsigned char version;
	unsigned int ofs_objects;
	
	unsigned int objects;
	unsigned char caches;
	unsigned long max_date;
	
	/* allocated space may be bigger than necessary for potential of 
	easy updating (if, eg., list is simply loaded into a hashmap) */
	unsigned char caches_buffer;
	unsigned char *cache_sha1s;
};

struct index_entry {
	unsigned char sha1[20];
	unsigned is_end : 1;
	unsigned cache_index : 7;
	unsigned int pos;
};


/* structure for actual cache file */
struct cache_slice_header {
	char signature[8]; /* REVCACHE */
	unsigned char version;
	unsigned int ofs_objects;
	
	unsigned int objects;
	unsigned short path_nr;
	unsigned int size;
	
	unsigned char pack_sha1[20];
};

struct object_entry {
	unsigned type : 3;
	unsigned is_start : 1;
	unsigned is_end : 1;
	unsigned stop_me : 1;
	unsigned uninteresting : 1;
	unsigned include : 1;
	unsigned char sha1[20];
	
	unsigned merge_nr : 6;
	unsigned split_nr : 7;
	unsigned size_size : 3;
	
	unsigned long date;
	unsigned short path;
	
	/* merge paths */
	/* split paths */
	/* size */
};

/* list resembles pack index format */
static unsigned int fanout[0xff + 2];

static unsigned char *idx_map = 0;
static int idx_size;
static struct index_header idx_head;

static struct strbuf *g_buffer;

#define SUPPORTED_REVCACHE_VERSION 		1
#define SUPPORTED_REVINDEX_VERSION		1

#define PATH_SIZE(x)	(sizeof(unsigned short) * (x))

#define OE_SIZE		sizeof(struct object_entry)
#define IE_SIZE		sizeof(struct index_entry)

#define OE_CAST(p)	((struct object_entry *)(p))
#define IE_CAST(p)	((struct index_entry *)(p))

#define ACTUAL_OBJECT_ENTRY_SIZE(e)		(OE_SIZE + PATH_SIZE((e)->merge_nr + (e)->split_nr) + (e)->size_size)

#define SLOP		5

/* initialization */

static int init_index(void)
{
	int fd;
	struct stat fi;
	
	fd = open(git_path("rev-cache/index"), O_RDONLY);
	if (fd == -1 || fstat(fd, &fi))
		goto end;
	if (fi.st_size < sizeof(struct index_header))
		goto end;
	
	idx_size = fi.st_size;
	idx_map = mmap(0, idx_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (idx_map == MAP_FAILED)
		goto end;
	if (get_index_head(idx_map, fi.st_size, &idx_head, fanout))
		goto end;
	
	atexit(cleanup_cache_slices);
	
	return 0;
	
end:
	return -1;
}

/* this assumes index is already loaded */
static struct index_entry *search_index(unsigned char *sha1)
{
	int start, end, starti, endi, i, len, r;
	struct index_entry *ie;
	
	if (!idx_map)
		return 0;
	
	/* binary search */
	start = fanout[(int)sha1[0]];
	end = fanout[(int)sha1[0] + 1];
	len = (end - start) / IE_SIZE;
	if (!len || len * IE_SIZE != end - start)
		return 0;
	
	starti = 0;
	endi = len - 1;
	for (;;) {
		i = (endi + starti) / 2;
		ie = IE_CAST(idx_map + start + i * IE_SIZE);
		r = hashcmp(sha1, ie->sha1);
		
		if (r) {
			if (starti + 1 == endi) {
				starti++;
				continue;
			} else if (starti == endi)
				break;
			
			if (r > 0)
				starti = i;
			else /* r < 0 */
				endi = i;
		} else
			return ie;
	}
	
	return 0;
}

unsigned char *get_cache_slice(unsigned char *sha1)
{
	struct index_entry *ie;
	
	if (!idx_map)
		init_index();
	
	ie = search_index(sha1);
	
	if (ie && ie->cache_index < idx_head.caches)
		return idx_head.cache_sha1s + ie->cache_index * 20;
	
	return 0;
}

