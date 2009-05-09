#include "cache.h"
#include "commit.h"

/*

if revs.only_hashes is set:
(or perhaps if revs.not_just_hashes is not set)

while traversing:
is this contained in a cache?
if so -> get and save (if not already) bitmap 
 -> traverse bitmap with following rules:

initial     | come across   | action
------------+---------------+------------ 
I           | I             | continue
I           | U             | throw away commit list (queue) but keep tree/blob list (pending)*
U           | I             | continue
U           | U             | continue
(I = interesting, U = not so)
* valid b/c of topo-ordering


each commit proceeds objects introduced as of that commit
such objects have a FACE_VALUE flag -> no tree walking

if per-slice bundles used -> let pack-files know where 
packed version of object is


*/

#define FACE_VALUE /* some value... */

/* single index maps objects to cache files */
static struct index_header {
	char signature[8]; /* REVINDEX */
	unsigned char version;
	unsigned int ofs_objects;
	
	unsigned int objects;
	unsigned char caches;
};

/* allocated space may be bigger than necessary for potential of 
easy updating (if, eg., list is simply loaded into a hashmap) */
unsigned int caches_buffer;
unsigned char *cache_sha1s;

/* list resembles pack index format */
unsigned int fanout[0xff + 1];

static struct index_entry {
	unsigned char sha1[20];
	unsigned has_bitmap : 1;
	unsigned cache_index : 7;
};


/* structure for actual cache file */
static struct cache_header {
	char signature[8]; /* REVCACHE */
	unsigned char version;
	unsigned int ofs_objects;
	unsigned int ofs_bitmaps;
	
	unsigned int objects;
	unsigned short starts;
	unsigned short ends;
	
	/* the below is filled in at run-time, and is effectively empty 
	 * space on the disk */
	unsigned char *start_sha1s; /* binary sha1 ++ binary sha1 ++ ... */
	unsigned char *end_sha1s;
	
	unsigned int size;
};

/* the size field might be screwy, but we need entries to have 
a fixed size; we could use two ints... */
static struct object_entry {
	unsigned type : 3;
	unsigned is_start : 1;
	unsigned flags : 4; /* for later */
	unsigned char sha1[20];
	unsigned int size;
};

/* at end for easy addition */
static struct bitmap_entry {
	unsigned char sha1[20];
	unsigned int z_size; /* compressed, in bytes */
	unsigned char *bitmap;
};

#define SUPPORTED_REVCACHE_VESION 		1

/* yes it makes the ordering a bit weird, but it's simpler */
#define SET_BIT(b, i)	((b)[(i) >> 3] |= 1 << ((i) & 0x7))
#define GET_BIT(b, i)	!!((b)[(i) >> 3] &  1 << ((i) & 0x7))


/* this will happen so rarely with so few that it really dosn't matter how we do it */
static int in_sha1_list(const unsigned char *list, int n, const unsigned char *sha1)
{
	int i;
	
	for (i = 0; i < n; i++) {
		if (list[i * 20] != *sha1 || memcmp(&list[i * 20], sha1, 20))
			continue;
		
		return 1;
	}
	
	return 0;
}

static int compress_bitmap(unsigned char *bitmap, int size)
{
	/* todo */
	
	return size;
}

static int deflate_bitmap(unsigned char *bitmap, int z_size)
{
	/* todo */
	
	return z_size;
}

static int make_bitmap(struct cache_header *head, unsigned char *map, struct commit *end, struct bitmap_entry *bitmap_entry)
{
	unsigned char *bitmap;
	struct commit_list *list, *queue;
	struct commit_list **listp, **queuep;
	struct commit *cur_commit;
	int bytesize, bitsize, allocated;
	struct object_entry *object;
	
	/* quickly obtain all objects reachable from end up through starts */
	queue = list = 0;
	listp = &list, queuep = &queue;
	queuep = &commit_list_insert(end, queuep)->next;
	while(queue) {
		struct commit_list *parents;
		struct commit *toadd;
		
		toadd = pop_commit(&queue);
		if (in_sha1_list(head->start_sha1s, head->starts, toadd->sha1))
			continue;
		listp = &commit_list_insert(toadd, listp)->next;
		
		parents = toadd->parents;
		while (parents) {
			struct commit *p = parents->item;
			
			parse_commit(p);
			queuep = &commit_list_insert(p, queuep)->next;
			parents = parents->next;
		}
	}
	
	/* with ordering the same, we need only step through cached list */
	sort_in_topological_order(&list, 1);
	cur_commit = pop_commit(&list);
	
	bitsize = head->objects;
	bitmap = xcalloc(bitsize / 8 + 1, 1);
	for (i = 0, index = head->ofs_objects; 
		i < head->objects; 
		i++, index += sizeof(object_entry)
	) {
		object = (struct object_entry *)(map + index);
		
		if (object->type != OBJ_COMMIT)
			continue;
		if (memcmp(object->sha1, cur_commit->sha1, 20)) 
			continue;
		
		SET_BIT(bitmap, i);
		cur_commit = pop_commit(&list);
	}
	
	memcpy(bitmap_entry->sha1, end->sha1, 20);
	bitmap_entry->bitmap = bitmap;
	bitmap_entry->z_size = 0;
	
	return 0;
}

static int get_bitmap(struct cache_header *head, unsigned char *map, struct commit *end, struct bitmap_entry *bitmap)
{
	struct bitmap_entry *be;
	int index = head->ofs_bitmaps;
	
	/* the -sizeof(..) is just for extra safety... */
	while (index < head->size - sizeof(struct bitmap_entry)) {
		be = (struct bitmap_entry *)(map + index);
		
		if (!memcmp(be->sha1, end->sha1, 20))
			break;
		
		index += sizeof(struct bitmap_entry) + be->z_size;
	}
	
	if (index >= head->size - sizeof(struct bitmap_entry))
		return 1;
	
	memcpy(bitmap->sha1, be->sha1, 20);
	bitmap->z_size = be->z_size;
	
	bitmap->bitmap = xcalloc(head->objects / 8 + 1, 1);
	memcpy(bitmap->bitmap, be + sizeof(struct bitmap_entry), bitmap->z_size);
	if (deflate_bitmap(bitmap->bitmap, bitmap->z_size) < head->objects / 8)
		return -1;
	
	return 0;
}

static int get_cache_header(unsigned char *map, int len, struct cache_header *head)
{
	int t;
	
	memcpy(head, map, sizeof(struct cache_header *));
	if (memcmp(head->signature, "REVCACHE", 8))
		return -1
	if (head->version > SUPPORTED_REVCACHE_VERSION)
		return -2;
	
	t = sizeof(struct cache_header) + (head->starts + head->ends) * 20;
	if (t != head->ofs_objects || t >= len)
		return -3;
	
	/* we only ever use the start hashes anyhow... */
	head->start_sha1s = xcalloc(head->starts, 20);
	memcpy(head->start_sha1s, map + sizeof(struct cache_header), head->starts * 20);
	
	head->end_sha1s = 0;
	head->size = len;
	
	return 0;
}

static int traverse_cache_slice_1(struct rev_info *revs, struct cache_header *head, 
	unsigned char *map, unsigned char *bitmap, unsigned char *used_bitmap, struct commit *commit, 
	struct commit_list **queue, struct commit_list **work)
{
	struct commit_list *q = 0, *w = 0;
	struct commit_list **qp = &q, **wp = &w;
	int i, index, retval = -2;
	char consuming_children = 0; /* that's right, this function is the evil twin */
	
	for (i = 0, index = head->ofs_object; i < head->objects; i++, index += sizeof(object_entry)) {
		struct object_entry *entry;
		struct object *object;
		
start_loop:
		if (!consuming_children && (!GET_BIT(bitmap, i) || GET_BIT(used_bitmap, i)))
			continue;
		
		entry = (struct object_entry *)(map + index);
		if (consuming_children) {
			/* children are straddled between adjacent commits */
			if (entry->type == OBJ_COMMIT) {
				consuming_children = 0;
				goto start_loop;
			}
			
			object = lookup_unknown_object(entry->sha1);
			object->type = entry->type;
			object->flags = FACE_VALUE;
			
			add_pending_object(revs, object, "");
			continue;
		}
		
		object = lookup_commit(entry->sha1);
		if (commit->flags & UNINTERESTING)
			object->flags |= UNINTERESTING;
		else if (object->flags & UNINTERESTING) {
			/* without knowing anything more about the topology, we're just gonna have to 
			 * ditch the commit stuff.  we can, however, keep all our non-commits
			 */
			goto end;
		}
		
		if (entry->is_start)
			wp = &commit_list_insert(object, wp)->next;
		else if (!(object->flags & UNINTERESTING) || revs->show_all)
			qp = &commit_list_insert(object, qp)->next;
	}
	
	/* this if shouldn't actually eval as true, b/c we're expecting queue to be generated as LIFO */
	if (*queue) {
		while ((*queue)->next) 
			*queue = (*queue)->next;
		(*queue)->next = q;
	} else 
		*queue = q;
	
	while (w)
		insert_by_date(pop_commit(&w), work);
	
	retval = 0;
	
end:
	free_commit_list(q);
	free_commit_list(w);
	
	return retval;
}


/* revs, which cache, object sha1, queue list, work list */
int traverse_cache_slice(struct rev_info *revs, unsigned char *cache_sha1, 
	struct commit *commit, struct commit_list **queue, struct commit_list **work)
{
	int fd = -1, made, retval = -1;
	struct stat fi;
	struct bitmap_entry bitmap;
	struct cache_header head;
	unsigned char *map = MAP_FAILED;
	
	/* todo: save head info and reload off that */
	memset(bitmap, 0, sizeof(bitmap_entry));
	memset(head, 0, sizeof(cache_header));
	
	fd = open(git_path("rev-cache/%s", cache_sha1), O_RDWR);
	if (fd == -1)
		goto end;
	if (fstat(fd, &fi) || fi.st_size < sizeof(struct cache_header))
		goto end;
	
	map = xmmap(0, fi.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED)
		goto end;
	if (get_cache_header(map, fi.st_size, &head))
		goto end;
	
	made = get_bitmap(&head, map, commit, &bitmap);
	if (made < 0)
		goto end;
	else if (made > 0) {
		make_bitmap(&head, map, commit, &bitmap);
		bitmap.z_size = compress_bitmap(bitmap.bitmap, head.objects / 8 + 1);
		
		/* yes we really are writing the useless pointer address too */
		lseek(fd, fi.st_size, SEEK_SET);
		write(fd, &bitmap, sizeof(struct bitmap_entry));
		write(fd, bitmap->bitmap, bitmap->z_size);
		
		deflate_bitmap(bitmap.bitmap, bitmap.z_size);
	}
	
	retval = traverse_cache_slice_1(revs, head, map, bitmap.bitmap, 0, commit, queue, work);
	
end:
	if (head.start_sha1s)
		free(head.start_sha1s);
	if (bitmap.bitmap)
		free(bitmap.bitmap);
	
	if (map != MAP_FAILED)
		unmap(map, fi.st_size);
	if (fd != -1)
		close(fd);
	
	return retval;
}


struct rev_cache {
	unsigned char sha1[20];
	unsigned char *bitmaps; /* collapsed */
	/* file head/map ? */
};

static struct object_entry2 {
	unsigned type : 3;
	unsigned is_start : 1;
	unsigned merges : 1;
	unsigned splits : 1;
	unsigned flags : 2;
	unsigned char sha1[20];
	unsigned int paths, stoppaths; /* the branch path(s) this object lies in */
};

static int traverse_cache_slice_2(...)
{
	unsigned int paths;
	
	/* first find end commit */
	for (i = head->ofs_objects; ; i += sizeof(object_entry2)) {
		object = (struct object_entry2 *)(map + i);
		if (object->type != OBJ_COMMIT)
			continue;
		if (object->sha1[0] != end->sha1[0])
			continue;
		if (memcmp(object->sha1, end->sha1, 20))
			continue;
		
		break;
	}
	
	paths = object->paths;
	for (i += sizeof(object_entry2); i < head->ofs_bitmaps; i += sizeof(object_entry2)) {
		object = (struct object_entry2 *)(map + i);
		
		if (!(object->paths & paths))
			continue;
		
		if (object->splits)
			paths &= ~object->stoppaths;
		else
			paths |= object->paths;
		
		/* ... */
	}
	
}



int make_cache_slice(struct commit_list **ends, struct commit_list **starts)
{
	
}

static int get_index_head(unsigned char *map, int len, struct index_header *head, unsigned int *fanout)
{
	int index = sizeof(struct index_header);
	
	memcpy(head, map, sizeof(struct index_header));
	if (len < index + head->caches_buffer * 20 + (0x100) * sizeof(unsigned int))
		return -1;
	
	head->cache_sha1s = xmalloc(head->caches * 20);
	memcpy(head->cache_sha1s, map + index, head->caches * 20);
	index += head->caches_buffer * 20;
	
	memcpy(fanout, map + index, (0x100) * sizeof(unsigned int));
	fanout[0x100] = len;
	
	return 0;
}

static int init_index(void)
{
	int fd;
	unsigned char *map;
	struct index_header *head;
	
	/* todo: store mapping/fanout for re-use */
	fd = open(git_path("rev-cache/index"), O_RDONLY);
	if (fd == -1 || lstat(fd, &fi))
		goto end;
	if (fi.st_size < sizeof(head))
		goto end;
	
	map = mmap(fd, fi.st_size, PROT_READ, MAP_PRIVATE, 0);
	if (map == MAP_FAILED)
		goto end;
	if (get_index_head(map, fi.st_size, &head, &fanout))
		goto end;
	
	atexit(...);
	
end:
	
	
}

const char *in_cache_slice(struct commit *commit)
{
	int start, end, len, i;
	struct index_entry *ie;
	
	/* binary search */
	start = fanout[commit->sha1[0]];
	end = fanout[commit->sha1[0] + 1];
	len = (end - start) / sizeof(struct index_entry);
	if (!len || len * sizeof(struct index_entry) != end - start)
		return 0;
	
	i = len / 2;
	do {
		ie = (struct index_entry *)(map + start + i * sizeof(struct index_entry));
		retval = memcmp(ie->sha1, commit->sha1, 20);
		
		if (r > 0) {
			len /= 2;
			i += len;
		} else if (r < 0) { 
			len /= 2;
			i -= len;
		} else {
			if (ie->cache_index < head->caches)
				return head->cache_sha1s[ie->cache_index];
			else
				return 0;
		}
	} while(len > 1);
	
	return 0;
}


/* single index maps objects to cache files */
static struct index_header {
	char signature[8]; /* REVINDEX */
	unsigned char version;
	unsigned int ofs_objects;
	
	unsigned int objects;
	unsigned char caches;
	
	/* allocated space may be bigger than necessary for potential of 
	easy updating (if, eg., list is simply loaded into a hashmap) */
	unsigned char caches_buffer;
	unsigned char *cache_sha1s;
};

/* list resembles pack index format */
unsigned int fanout[0xff + 2];

static struct index_entry {
	unsigned char sha1[20];
	unsigned has_bitmap : 1;
	unsigned cache_index : 7;
};