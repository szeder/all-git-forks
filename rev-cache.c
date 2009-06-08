#include <assert.h>
#include "cache.h"
#include "object.h"
#include "commit.h"
#include "tree.h"
#include "tree-walk.h"
#include "diff.h"
#include "revision.h"
#include "run-command.h"

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


each commit preceeds objects introduced as of that commit
such objects have a FACE_VALUE flag -> no tree walking

if per-slice bundles used -> let pack-files know where 
packed version of object is (-> no searching necessary)

NOTE: end/start are chronological references, not toplogical
      in retrospect this should probably change...

*/

#define FACE_VALUE 		0x100 /* some value... (to be determined during integration) */

/* single index maps objects to cache files */
struct index_header {
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
static unsigned int fanout[0xff + 2];

struct index_entry {
	unsigned char sha1[20];
	unsigned is_end : 1;
	unsigned cache_index : 7;
};


/* structure for actual cache file */
struct cache_slice_header {
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
	unsigned char pack_sha1[20];
};

/* the size field might be screwy, but we need entries to have 
a fixed size; we could use two ints... */
struct object_entry {
	unsigned type : 3;
	unsigned is_start : 1;
	unsigned is_end : 1;
	unsigned flags : 3; /* for later */
	unsigned char sha1[20];
	unsigned int ofs_pack;
	/* unsigned int size; */
};

/* at file end for easy addition */
struct bitmap_entry {
	unsigned char sha1[20];
	unsigned int z_size; /* compressed, in bytes */
	unsigned char *bitmap;
};

static unsigned char *idx_map = 0;
static int idx_size;
static struct index_header idx_head;

static struct strbuf *g_buffer;

static char dont_pack_it = 0;

struct pack_entry **pack_slice = 0;
static int pack_slice_nr = 0, pack_slice_sz = 0;

#define SUPPORTED_REVCACHE_VERSION 		1
#define SUPPORTED_REVINDEX_VERSION		1

/* yes it makes the ordering a bit weird, but it's simpler */
#define SET_BIT(b, i)	((b)[(i) >> 3] |= 1 << ((i) & 0x7))
#define GET_BIT(b, i)	!!((b)[(i) >> 3] &  1 << ((i) & 0x7))
#define BITMAP_SIZE(os)	((os) / 8 + 1)

#define OE_SIZE	sizeof(struct object_entry)
#define IE_SIZE	sizeof(struct index_entry)

#define OE_CAST(p)	((struct object_entry *)(p))
#define IE_CAST(p)	((struct index_entry *)(p))

static int make_cache_index(const char *, struct strbuf *);
static unsigned char *make_cache_pack(char *, struct strbuf *);
static int init_index(void);
static void cleanup_cache_slices(void);
static struct index_entry *search_index(unsigned char *);

#define MOVE_SLICE_HASH(h)			(((h) + 1) * ((h) + 1) % pack_slice_sz)

static unsigned int pack_slice_hash(unsigned char *sha1)
{
	unsigned int h = 0;
	
	h |= (unsigned int)sha1[0] << 24;
	h |= (unsigned int)sha1[1] << 16;
	h |= (unsigned int)sha1[2] << 8;
	h |= (unsigned int)sha1[3];
	
	return h % pack_slice_sz;
}

struct pack_entry *get_pack_slice_entry(unsigned char *sha1)
{
	unsigned int h = pack_slice_hash(sha1);
	
	if (!pack_slice)
		return 0;
	
	while (pack_slice[h]) {
		struct pack_entry *pe = pack_slice[h];
		
		if (!hashcmp(pe->sha1, sha1))
			return pe;
		
		h = MOVE_SLICE_HASH(h);
	}
	
	return 0;
}

#ifndef max
#	define max(a, b)		((a) > (b) ? (a) : (b))
#endif

static void expand_pack_slice(void)
{
	struct pack_entry **new_slice;
	int old_size;
	
	if (pack_slice_nr * 4 < pack_slice_sz * 3)
		return;
	
	old_size = pack_slice_sz;
	pack_slice_sz += max(0xffff + 1, pack_slice_nr);
	new_slice = xcalloc(pack_slice_sz, sizeof(struct packed_entry *));
	
	if (pack_slice) {
		int i;
		
		/* re-locate hashes */
		for (i = 0; i < old_size; i++) {
			struct pack_entry *pe;
			unsigned int h;
			
			if (!pack_slice[i])
				continue;
			
			pe = pack_slice[i];
			h = pack_slice_hash(pe->sha1);
			while (new_slice[h])
				h = MOVE_SLICE_HASH(h);
			
			new_slice[h] = pe;
		}
		
		free(pack_slice);
	}
	
	pack_slice = new_slice;
	
}

static int add_pack_slice_entry(unsigned char *sha1, off_t offset, struct packed_git *pack)
{
	struct pack_entry *entry;
	unsigned int h = pack_slice_hash(sha1);
	
	expand_pack_slice();
	
	while (pack_slice[h]) {
		struct pack_entry *pe = pack_slice[h];
		
		if (!hashcmp(pe->sha1, sha1))
			break;
		
		h = MOVE_SLICE_HASH(h);
	}
	
	if (pack_slice[h]) 
		return 1;
	
	entry = xcalloc(1, sizeof(struct pack_entry));
	
	hashcpy(entry->sha1, sha1);
	entry->offset = offset;
	entry->p = pack;
	pack_slice[h] = entry;
		
	return 0;
}

/* this will happen so rarely with so few that it really dosn't matter how we do it */
static int in_sha1_list(const unsigned char *list, int n, const unsigned char *sha1)
{
	int i;
	
	for (i = 0; i < n; i++) {
		if (list[i * 20] != sha1[0] || hashcmp(&list[i * 20], sha1))
			continue;
		
		return 1;
	}
	
	return 0;
}

/*

x00NNNNN - as is
x01NNNNN - not yet assigned
x10NNNNN - 0s
x11NNNNN - 1s

*/

static int encode_size(unsigned int size, unsigned char *out)
{
	int i;
	
	for (i = 0; size; i++) {
		if (!i) {
			*out = (unsigned char)(size & 0x1f);
			size >>= 5;
		} else {
			*out = (unsigned char)(size & 0x7f);
			size >>= 7;
		}
		
		if (size)
			*out++ |= 0x80;
		else
			*out++;
	}
	
	return i;
}

static int decode_size(unsigned char *s, int len, unsigned char **p)
{
	unsigned int size;
	int i, shift;
	
	size = 0;
	shift = 0;
	for (i = 0; i < len; i++) {
		if (!i) {
			size = (unsigned int)(*s & 0x1f);
			shift = 5;
		} else {
			size |= (unsigned int)(*s & 0x7f) << shift;
			shift += 7;
		}
		
		if (!(*s & 0x80)) {
			if (p)
				*p = s + 1;
			return size;
		} else
			s++;
	}
	
	return 0;
}

/* uses a very simple rle format */
static int compress_bitmap(unsigned char **pbitmap, int size)
{
	int i, j, k;
	struct strbuf out;
	unsigned char what[5];
	unsigned char compress, *bitmap;
	int byte_size; /* hehe */
	
	bitmap = *pbitmap;
	strbuf_init(&out, 0);
	
	i = j = k = 0;
	compress = 0;
	while (i < size) {
		/* random stuff */
		while (j < size && bitmap[j] != 0x00 && bitmap[j] != 0xff)
			j++;
		
		if (j == size)
			goto skip_compress;
		
		/* try 1s */
		compress = 0;
		for (k = j; k < size && bitmap[k] == 0xff; k++) ;
		
		if (k - j > 2) {
			compress = 0x11;
			goto skip_compress;
		}
		
		/* try 0s */
		for (k = j; k < size && bitmap[j] == 0x00; k++) ;
		
		if (k - j > 2) {
			compress = 0x10;
			goto skip_compress;
		}
		
skip_compress:
		if (j - i) {
			byte_size = encode_size(j - i, what);
			strbuf_add(&out, what, byte_size);
			strbuf_add(&out, bitmap + i, j - i);
			
			i = j;
		}
		
		if (compress) {
			byte_size = encode_size(k - j, what);
			what[0] |= compress << 6;
			strbuf_add(&out, what, byte_size);
			
			i = k;
		}
	}
	
	free(bitmap);
	*pbitmap = strbuf_detach(&out, &byte_size);
	
	return byte_size;
}

#ifndef min
#	define min(a, b)		((a) < (b) ? (a) : (b))
#endif

static int deflate_bitmap(unsigned char **pbitmap, int z_size)
{
	unsigned char what, *bitmap, *p;
	struct strbuf out;
	int i, size;
	unsigned char buffer[100];
	
	bitmap = *pbitmap;
	strbuf_init(&out, 0);
	
	i = 0;
	while (i < z_size) {
		what = bitmap[i] >> 6 & 0x02;
		
		size = decode_size(bitmap + i, z_size - i, &p);
		if (!size)
			goto fail;
		i = p - bitmap;
		
		switch (what) {
		case 0x00 : 
			if (i + size > z_size)
				goto fail;
			
			strbuf_add(&out, bitmap + i, size);
			i += size;
			break;
		case 0x01 : 
			goto fail;
		case 0x10 : 
			memset(buffer, 0x00, sizeof(buffer));
			goto write_block;
		case 0x11 : 
			memset(buffer, 0xff, sizeof(buffer));
write_block:
			while (size > 0) {
				strbuf_add(&out, buffer, min(size, sizeof(buffer)));
				size -= sizeof(buffer);
			}
		}
	}
	
	free(bitmap);
	*pbitmap = strbuf_detach(&out, &size);
	
	return size;
	
fail:
	strbuf_release(&out);
	
	return 0;
}

#define HEARD		TMP_MARK

static int make_bitmap(struct cache_slice_header *head, unsigned char *map, struct commit *end, struct bitmap_entry *bitmap_entry)
{
	unsigned char *bitmap;
	struct commit_list *list, *queue;
	struct commit_list **listp, **queuep;
	struct commit *cur_commit;
	struct object_entry *object;
	int i, index;
	
	/* just to be sure that we have it... */
	parse_commit(end);
	
	/* quickly obtain all objects reachable from end up through starts; 
	 * can't use internal revision walker w/o starting a new thread b/c (obviously) we're 
	 * in a revision walk */
	queue = list = 0;
	listp = &list, queuep = &queue;
	queuep = &commit_list_insert(end, queuep)->next;
	for (; queue; pop_commit(&queue)) {
		struct commit_list *parents;
		struct commit *toadd;
		
		toadd = queue->item;
		listp = &commit_list_insert(toadd, listp)->next;
		if (in_sha1_list(head->start_sha1s, head->starts, toadd->object.sha1))
			continue;
		
		/* printf("adding %s\n", sha1_to_hex(toadd->object.sha1)); */
		for (parents = toadd->parents; parents; parents = parents->next) {
			struct commit *p = parents->item;
			
			/* we gotta be careful to clean these flags after use... */
			if (p->object.flags & HEARD)
				continue;
			
			parse_commit(p);
			p->object.flags |= HEARD;
			queuep = &commit_list_insert(p, queuep)->next;
		}
	}
	
	/* with ordering the same, we need only step through cached list */
	sort_in_topological_order(&list, 1);
	
	cur_commit = pop_commit(&list);
	bitmap = xcalloc(BITMAP_SIZE(head->objects), 1);
	for (i = 0, index = head->ofs_objects; 
		i < head->objects && cur_commit; 
		i++, index += OE_SIZE
	) {
		object = OE_CAST(map + index);
		
		if (object->type != OBJ_COMMIT)
			continue;
		if (hashcmp(object->sha1, cur_commit->object.sha1)) 
			continue;
		
		SET_BIT(bitmap, i);
		cur_commit->object.flags &= ~HEARD; /* we're very tidy! */
		cur_commit = pop_commit(&list);
	}
	
	/* this should never happen! */
	/* assert(!cur_commit); */
	if (cur_commit) {
		free(bitmap);
		return -1;
	}
	
	hashcpy(bitmap_entry->sha1, end->object.sha1);
	bitmap_entry->bitmap = bitmap;
	bitmap_entry->z_size = 0; /* set in caller */
	
	return 0;
}

static int get_bitmap(struct cache_slice_header *head, unsigned char *map, struct commit *end, struct bitmap_entry *bitmap)
{
	struct bitmap_entry *be = 0;
	int index = head->ofs_bitmaps;
	
	/* the -sizeof(..) is just for extra safety... */
	while (index < head->size - sizeof(struct bitmap_entry)) {
		be = (struct bitmap_entry *)(map + index);
		
		if (!hashcmp(be->sha1, end->object.sha1))
			break;
		
		index += sizeof(struct bitmap_entry) + ntohl(be->z_size);
	}
	
	if (index >= head->size - sizeof(struct bitmap_entry))
		return 1;
	
	hashcpy(bitmap->sha1, be->sha1);
	bitmap->z_size = ntohl(be->z_size);
	
	bitmap->bitmap = xcalloc(BITMAP_SIZE(head->objects), 1);
	memcpy(bitmap->bitmap, map + index + sizeof(struct bitmap_entry), bitmap->z_size);
	if (deflate_bitmap(&bitmap->bitmap, bitmap->z_size) != BITMAP_SIZE(head->objects))
		return -1;
	
	return 0;
}

static int get_cache_slice_header(unsigned char *map, int len, struct cache_slice_header *head)
{
	int t;
	
	memcpy(head, map, sizeof(struct cache_slice_header));
	head->ofs_objects = ntohl(head->ofs_objects);
	head->ofs_bitmaps = ntohl(head->ofs_bitmaps);
	head->objects = ntohl(head->objects);
	head->starts = ntohs(head->starts);
	head->ends = ntohs(head->ends);
	head->size = ntohl(head->size);
	head->start_sha1s = head->end_sha1s = 0;
	if (memcmp(head->signature, "REVCACHE", 8))
		return -1;
	if (head->version > SUPPORTED_REVCACHE_VERSION)
		return -2;
	
	t = sizeof(struct cache_slice_header) + (head->starts + head->ends) * 20;
	if (t != head->ofs_objects || t >= len)
		return -3;
	
	/* we only ever use the start hashes anyhow... */
	head->start_sha1s = xcalloc(head->starts, 20);
	memcpy(head->start_sha1s, map + sizeof(struct cache_slice_header), head->starts * 20);
	
	head->end_sha1s = 0;
	head->size = len;
	
	return 0;
}

static int traverse_cache_slice_1(struct rev_info *revs, struct cache_slice_header *head, 
	unsigned char *map, unsigned char *bitmap, struct commit *commit, 
	struct commit_list ***queue, struct commit_list **work, struct packed_git *pack)
{
	struct commit_list *q = 0, *w = 0;
	struct commit_list **qp = &q, **wp = &w;
	int i, index, retval = -2;
	char use_objects = 1, consuming_children = 0; /* that's right, this function is the evil twin */
	unsigned char *anti_bitmap = 0;
	struct bitmap_entry be;
	
	for (i = 0; !bitmap[i]; i++) ;
	
	for (index = head->ofs_objects + i * OE_SIZE; i < head->objects; i++, index += OE_SIZE) {
		struct object_entry *entry;
		struct object *object;
		struct commit *co;
		
start_loop:
		if (!consuming_children) {
			if (anti_bitmap && GET_BIT(anti_bitmap, i))
				continue;
			else if (!GET_BIT(bitmap, i))
				continue;
		}
		
		entry = OE_CAST(map + index);
		if (consuming_children) {
			/* children are straddled between adjacent commits */
			switch (entry->type) {
			case OBJ_COMMIT : 
				consuming_children = 0;
				goto start_loop;
			case OBJ_TAG : 
				if (!revs->tag_objects)
					goto skip_object;
				break;
			case OBJ_TREE : 
				if (!revs->tree_objects)
					goto skip_object;
				break;
			case OBJ_BLOB : 
				if (!revs->blob_objects)
					goto skip_object;
				break;
			}
			
			object = lookup_unknown_object(entry->sha1);
			object->type = entry->type;
			
			/* in the special case where we can handle a mid-slice uninteresting commit 
			 * we won't be able to rely the preceeding's cached sub-commit objects */
			if (use_objects)
				object->flags = FACE_VALUE;
			
			if (revs->for_pack && pack)
				add_pack_slice_entry(entry->sha1, ntohl(entry->ofs_pack), pack);
			
			add_pending_object(revs, object, "");
skip_object:
			continue;
		} else if (entry->type != OBJ_COMMIT)
			continue;
		
		co = lookup_commit(entry->sha1);
		object = &co->object;
		if (commit->object.flags & UNINTERESTING)
			object->flags |= UNINTERESTING;
		else if (object->flags & UNINTERESTING) {
			/* without knowing anything more about the topology, we're just gonna have to 
			 * ditch the commit stuff.  we can, however, keep all our non-commits */
			/* should we perhaps make the bitmap if we can't find it? */
			if (get_bitmap(head, map, co, &be)) {
				retval = -1;
				goto end;
			}
			
			/* then again, if we *do* know something more, we can use it! */
			if (anti_bitmap) {
				int j;
				
				/* wow! this is unusual. merge with what we've got */
				for(j = 0; j < BITMAP_SIZE(head->objects); j++)
					anti_bitmap[j] |= be.bitmap[j];
				
				free(be.bitmap);
			} else
				anti_bitmap = be.bitmap;
			
			consuming_children = 1;
			use_objects = 0; /* important! */
		}
		
		if (entry->is_start) {
			parse_commit(co);
			if (!(object->flags & SEEN) || object->flags & UNINTERESTING) {
				wp = &commit_list_insert(co, wp)->next; /* or mark_parents_uninteresting for other case? */
				if (!(object->flags & UNINTERESTING)) {
					consuming_children = use_objects = 1;
					if (revs->for_pack && pack)
						add_pack_slice_entry(entry->sha1, ntohl(entry->ofs_pack), pack);
				}
			}
		} else if (!(object->flags & UNINTERESTING) || revs->show_all) {
			if (!(object->flags & SEEN)) {
				object->flags |= SEEN;
				qp = &commit_list_insert(co, qp)->next;
				
				if (!(object->flags & UNINTERESTING)) {
					consuming_children = use_objects = 1;
					if (revs->for_pack && pack)
						add_pack_slice_entry(entry->sha1, ntohl(entry->ofs_pack), pack);
				}
			}
		}
	}
	
	/* queue is LIFO */
	if (!**queue)
		**queue = q;
	*queue = qp;
	
	while (w)
		insert_by_date(pop_commit(&w), work);
	
	retval = 0;
	
end:
	if (retval != 0) {
		free_commit_list(q);
		free_commit_list(w);
	}
	if (anti_bitmap)
		free(anti_bitmap);
	
	return retval;
}

struct rev_cache {
	unsigned char sha1[20];
	unsigned char *map;
	int size;
	
	struct cache_slice_header head;
	struct rev_cache *next;
};

extern struct packed_git *packed_git;

/* revs, which cache, object sha1, queue list, work list */
int traverse_cache_slice(struct rev_info *revs, unsigned char *cache_sha1, 
	struct commit *commit, struct commit_list ***queue, struct commit_list **work)
{
	int fd = -1, made, retval = -1;
	struct stat fi;
	struct bitmap_entry bitmap;
	struct cache_slice_header head;
	unsigned char *map = MAP_FAILED;
	char *path, *path2;
	struct packed_git *pack = 0;
	
	/* todo: save map/head info and reload off that */
	memset(&bitmap, 0, sizeof(struct bitmap_entry));
	memset(&head, 0, sizeof(struct cache_slice_header));
	
	fd = open(git_path("rev-cache/%s", sha1_to_hex(cache_sha1)), O_RDWR);
	if (fd == -1)
		goto end;
	if (fstat(fd, &fi) || fi.st_size < sizeof(struct cache_slice_header))
		goto end;
	
	map = xmmap(0, fi.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED)
		goto end;
	if (get_cache_slice_header(map, fi.st_size, &head))
		goto end;
	
	if (!is_null_sha1(head.pack_sha1)) {
		/* for some reason the packed_git struct stores the .pack name, but you gotta initialize it 
		 * with the .idx name... */
		path = git_path("rev-cache/%s-%s.idx", sha1_to_hex(cache_sha1), sha1_to_hex(head.pack_sha1));
		path2 = git_path("rev-cache/%s-%s.pack", sha1_to_hex(cache_sha1), sha1_to_hex(head.pack_sha1));
		
		for (pack = packed_git; pack; pack = pack->next) {
			if (!strcmp(pack->pack_name, path2))
				break;
		}
		
		if (!pack) {
			pack = add_packed_git(path, strlen(path), 1);
			if (pack)
				install_packed_git(pack);
		}
	}
	
	made = get_bitmap(&head, map, commit, &bitmap);
	if (made < 0)
		goto end;
	else if (made > 0) {
		make_bitmap(&head, map, commit, &bitmap);
		bitmap.z_size = htonl(compress_bitmap(&bitmap.bitmap, BITMAP_SIZE(head.objects)));
		
		/* yes we really are writing the useless pointer address too */
		lseek(fd, fi.st_size, SEEK_SET);
		write(fd, &bitmap, sizeof(struct bitmap_entry));
		
		bitmap.z_size = ntohl(bitmap.z_size);
		write(fd, bitmap.bitmap, bitmap.z_size);
		
		deflate_bitmap(&bitmap.bitmap, bitmap.z_size);
	}
	
	retval = traverse_cache_slice_1(revs, &head, map, bitmap.bitmap, commit, queue, work, pack);
	
end:
	if (head.start_sha1s)
		free(head.start_sha1s);
	if (bitmap.bitmap)
		free(bitmap.bitmap);
	
	if (map != MAP_FAILED)
		munmap(map, fi.st_size);
	if (fd != -1)
		close(fd);
	
	return retval;
}



/* this needs to be checked explicitly to ensure a clean cut */
static int is_start(struct commit *commit)
{
	struct commit_list *list;
	
	if (commit->object.flags & UNINTERESTING) 
		return 0;
	
	for (list = commit->parents; list; list = list->next)
		if (!(list->item->object.flags & UNINTERESTING))
			return 0;
	
	return 1;
}

static void add_object_entry(const char *sha1, int type, struct object_entry *nothisone)
{
	struct object_entry object;
	
	if (!nothisone) {
		memset(&object, 0, sizeof(object));
		object.type = type;
		hashcpy(object.sha1, sha1);
		
		nothisone = &object;
	}
	
	/* we're not going to worry about endianness for now */
	strbuf_add(g_buffer, nothisone, sizeof(object));
}

static void tree_addremove(struct diff_options *options,
	int whatnow, unsigned mode,
	const unsigned char *sha1,
	const char *concatpath)
{
	if (whatnow != '+')
		return;
	
	/* printf("adding (%s) %s\n", S_ISDIR(mode) ? "OBJ_TREE" : "OBJ_BLOB", sha1_to_hex(sha1)); */
	add_object_entry(sha1, S_ISDIR(mode) ? OBJ_TREE : OBJ_BLOB, 0);
}

static void tree_change(struct diff_options *options,
	unsigned old_mode, unsigned new_mode,
	const unsigned char *old_sha1,
	const unsigned char *new_sha1,
	const char *concatpath)
{
	if (!hashcmp(old_sha1, new_sha1))
		return;
	
	/* printf("changing (%s) %s\n", S_ISDIR(new_mode) ? "OBJ_TREE" : "OBJ_BLOB", sha1_to_hex(new_sha1)); */
	add_object_entry(new_sha1, S_ISDIR(new_mode) ? OBJ_TREE : OBJ_BLOB, 0);
}

static int cache_sort_hash(const void *a, const void *b)
{
	return hashcmp(OE_CAST(a)->sha1, OE_CAST(b)->sha1);
}

static int cache_sort_type(const void *a, const void *b)
{
	struct object_entry *entry1, *entry2;
	
	entry1 = OE_CAST(a);
	entry2 = OE_CAST(b);
	
	if (entry1->type == entry2->type) 
		return 0;
	
	return entry1->type < entry2->type ? 1 : -1;
}

static int write_cache_slice(char *name, struct cache_slice_header *head, struct strbuf *body)
{
	struct cache_slice_header whead;
	int fd;
	
	fd = open(git_path("rev-cache/%s", name), O_CREAT | O_WRONLY, 0666);
	if (fd < 0)
		return -1;
	
	memcpy(&whead, head, sizeof(whead));
	whead.ofs_objects = htonl(whead.ofs_objects);
	whead.ofs_bitmaps = htonl(whead.ofs_bitmaps);
	whead.objects = htonl(whead.objects);
	whead.starts = htons(whead.starts);
	whead.ends = htons(whead.ends);
	whead.size = htonl(whead.size);
	write(fd, &whead, sizeof(whead));
	write_in_full(fd, head->start_sha1s, head->starts * 20);
	write_in_full(fd, head->end_sha1s, head->ends * 20);
	
	/* thankfully, also no endianness troubles per object */
	write_in_full(fd, body->buf, body->len);
	
	close(fd);
	
	return 0;
}


/* returns non-zero to continue parsing, 0 to skip */
typedef int (*dump_tree_fn)(const unsigned char *, const char *, unsigned int); /* sha1, path, mode */

/* we need to walk the trees by hash, so unfortunately we can't use traverse_trees in tree-walk.c */
static int dump_tree(struct tree *tree, dump_tree_fn fn)
{
	struct tree_desc desc;
	struct name_entry entry;
	struct tree *subtree;
	int r;
	
	if (parse_tree(tree))
		return -1;
	
	init_tree_desc(&desc, tree->buffer, tree->size);
	while (tree_entry(&desc, &entry)) {
		switch (fn(entry.sha1, entry.path, entry.mode)) {
		case 0 :
			goto continue_loop;
		default : 
			break;
		}
		
		if (S_ISDIR(entry.mode)) {
			subtree = lookup_tree(entry.sha1);
			if (!subtree)
				return -2;
			
			if ((r = dump_tree(subtree, fn)) < 0)
				return r;
		}
		
continue_loop:
		continue;
	}
	
	return 0;
}

static int dump_tree_callback(const unsigned char *sha1, const char *path, unsigned int mode)
{
	add_object_entry(sha1, S_ISDIR(mode) ? OBJ_TREE : OBJ_BLOB, 0);
	
	return 1;
}

/* assumes buffers are sorted */
static void object_set_difference(struct strbuf *set, struct strbuf *not)
{
	int i, j;
	
	for (i = j = 0; i < set->len; i += OE_SIZE) {
		while (j < not->len && cache_sort_hash(not->buf + j, set->buf + i) < 0) 
			j += OE_SIZE;
		
		if (j < not->len && !cache_sort_hash(not->buf + j, set->buf + i))
			continue;
		
		add_object_entry(0, 0, OE_CAST(set->buf + i));
	}
	
}

/* {commit objects} \ {parent objects also in slice (ie. 'interesting')} */
static int add_unique_objects(struct commit *commit, struct strbuf *out)
{
	struct commit_list *list;
	struct tree *first;
	struct strbuf os, us, *orig_buf;
	struct diff_options opts;
	int orig_len;
	
	strbuf_init(&os, 0);
	strbuf_init(&us, 0);
	orig_len = out->len;
	orig_buf = g_buffer;
	
	diff_setup(&opts);
	DIFF_OPT_SET(&opts, RECURSIVE);
	DIFF_OPT_SET(&opts, TREE_IN_RECURSIVE);
	opts.change = tree_change;
	opts.add_remove = tree_addremove;
	
	/* generate interesting parent list */
	first = 0;
	g_buffer = &os;
	for (list = commit->parents; list; list = list->next) {
		struct commit *parent = list->item;
		
		if (parent->object.flags & UNINTERESTING)
			continue;
		
		if (!first) {
			first = parent->tree;
			dump_tree(first, dump_tree_callback);
			continue;
		}
		
		diff_tree_sha1(first->object.sha1, parent->tree->object.sha1, "", &opts);
	}
	
	/* set difference */
	if (os.len) {
		qsort(os.buf, os.len / OE_SIZE, OE_SIZE, cache_sort_hash);
		
		g_buffer = &us;
		dump_tree(commit->tree, dump_tree_callback);
		qsort(us.buf, us.len / OE_SIZE, OE_SIZE, cache_sort_hash);
		
		g_buffer = out;
		object_set_difference(&us, &os);
	} else {
		g_buffer = out;
		dump_tree(commit->tree, dump_tree_callback);
	}
	
	/* 'topo' sort of sub-commit objects */
	if (out->len - orig_len)
		qsort(out->buf + orig_len, (out->len - orig_len) / OE_SIZE, OE_SIZE, cache_sort_type);
	
	/* ... */
	strbuf_release(&us);
	strbuf_release(&os);
	g_buffer = orig_buf;
	
	return 0;
}

/* 
 * because this is a self-contained branch slice, each commit must be either:
 * - contained entirely within slice (all parents interesting)
 * - a start commit (all parents uninteresting)
 * if this is not the case we gotta 'traverse half-contained' entries until it is
 */
static void make_legs(struct rev_info *revs)
{
	struct commit_list *list, **plist;
	int total = 0;
	
	/* attach plist to end of commits list */
	list = revs->commits;
	while (list && list->next)
		list = list->next;
	
	if (list)
		plist = &list->next;
	else
		return;
	
	/* duplicates don't matter, as get_revision() ignores them */
	for (list = revs->commits; list; list = list->next) {
		struct commit *item = list->item;
		struct commit_list *parents = item->parents;
		
		if (item->object.flags & UNINTERESTING)
			continue;
		if (is_start(item))
			continue;
		
		while (parents) {
			struct commit *p = parents->item;
			parents = parents->next;
			
			if (!(p->object.flags & UNINTERESTING))
				continue;
			
			p->object.flags &= ~UNINTERESTING;
			parse_commit(p);
			plist = &commit_list_insert(p, plist)->next;
			
			if (!(p->object.flags & SEEN))
				total++;
		}
	}
	
	if (total)
		sort_in_topological_order(&revs->commits, 1);
	
}

#define NOT_END			TMP_MARK

int make_cache_slice(struct rev_info *revs, struct commit_list **ends, struct commit_list **starts)
{
	struct commit_list *list;
	struct rev_info therevs;
	struct strbuf buffer, endlist, startlist;
	struct cache_slice_header head;
	struct commit *commit;
	unsigned char sha1[20], *sha1p;
	git_SHA_CTX ctx;
	
	strbuf_init(&buffer, 0);
	strbuf_init(&endlist, 0);
	strbuf_init(&startlist, 0);
	g_buffer = &buffer;
	
	if (!revs) {
		revs = &therevs;
		init_revisions(revs, 0);
		
		/* we're gonna assume no one else has already traversed this... */
		for (list = *ends; list; list = list->next)
			add_pending_object(revs, &list->item->object, 0);
		
		for (list = *starts; list; list = list->next) {
			list->item->object.flags |= UNINTERESTING;
			add_pending_object(revs, &list->item->object, 0);
		}
	}
	
	revs->tree_objects = 1;
	revs->blob_objects = 1;
	revs->topo_order = 1;
	revs->lifo = 1;
	
	setup_revisions(0, 0, revs, 0);
	if (prepare_revision_walk(revs))
		die("died preparing revision walk");
	
	make_legs(revs);
	
	/* using the very system we're optimizing may seem a bit weird, but as we 
	 * shouldn't ever be making cache slices _within_ a traversal we should be ok */
	while ((commit = get_revision(revs)) != 0) {
		struct object_entry object;
		
		memset(&object, 0, sizeof(object));
		object.type = OBJ_COMMIT;
		hashcpy(object.sha1, commit->object.sha1);
		
		/* determine if this is an endpoint: 
		 * if all parents are uninteresting -> start
		 * if this isn't a parent of a SEEN -> end */
		if (!(commit->object.flags & NOT_END)) {
			object.is_end = 1;
			strbuf_add(&endlist, commit->object.sha1, 20);
		} else
			commit->object.flags &= ~NOT_END;
		
		if (is_start(commit)) {
			object.is_start = 1;
			strbuf_add(&startlist, commit->object.sha1, 20);
		} else {
			/* parents are either all interesting or all uninteresting... */
			for (list = commit->parents; list; list = list->next)
				list->item->object.flags |= NOT_END;
		}
		
		/* printf("%s [%d]\n", sha1_to_hex(object.sha1), object.is_start); */
		add_object_entry(0, 0, &object);
		
		/* add all unique children for this commit */
		if (commit->object.flags & TREESAME) 
			continue;
		
		add_object_entry(commit->tree->object.sha1, OBJ_TREE, 0);
		add_unique_objects(commit, &buffer);
	}
	
	/* initialize header */
	memset(&head, 0, sizeof(head));
	strcpy(head.signature, "REVCACHE");
	head.version = SUPPORTED_REVCACHE_VERSION;
	
	head.starts = startlist.len / 20;
	head.start_sha1s = xmemdupz(startlist.buf, startlist.len);
	head.ends = endlist.len / 20;
	head.end_sha1s = xmemdupz(endlist.buf, endlist.len);
	
	head.ofs_objects = sizeof(head) + startlist.len + endlist.len;
	head.ofs_bitmaps = head.ofs_objects + buffer.len;
	head.objects = buffer.len / OE_SIZE;
	head.size = head.ofs_bitmaps;
	
	/* the meaning of the hash name is more or less irrelevant, it's the uniqueness that matters */
	strbuf_add(&startlist, endlist.buf, endlist.len);
	git_SHA1_Init(&ctx);
	git_SHA1_Update(&ctx, startlist.buf, startlist.len);
	git_SHA1_Final(sha1, &ctx);
	
	if (!dont_pack_it) {
		sha1p = make_cache_pack(sha1_to_hex(sha1), &buffer);
		hashcpy(head.pack_sha1, sha1p);
	}
	
	if (write_cache_slice(sha1_to_hex(sha1), &head, &buffer) < 0)
		die("write failed");
	
	if (make_cache_index(sha1, &buffer) < 0)
		die("can't update index");
	
	free(head.start_sha1s);
	free(head.end_sha1s);
	strbuf_release(&buffer);
	strbuf_release(&startlist);
	strbuf_release(&endlist);
	
	return 0;
}

/* todo: add a garbage cleaner to weed out unused stuff from slices and index */

static int index_sort_hash(const void *a, const void *b)
{
	return hashcmp(IE_CAST(a)->sha1, IE_CAST(b)->sha1);
}

/* todo: handle concurrency issues */
static int write_cache_index(struct strbuf *body)
{
	struct index_header whead;
	int fd, i;
	
	cleanup_cache_slices();
	
	fd = open(git_path("rev-cache/index"), O_CREAT | O_WRONLY, 0666);
	if (fd < 0)
		return -1;
	
	/* endianness yay! */
	memcpy(&whead, &idx_head, sizeof(whead));
	whead.ofs_objects = htonl(whead.ofs_objects);
	whead.objects = htonl(whead.objects);
	write(fd, &whead, sizeof(struct index_header));
	write_in_full(fd, idx_head.cache_sha1s, idx_head.caches_buffer * 20);
	
	for (i = 0; i <= 0xff; i++)
		fanout[i] = htonl(fanout[i]);
	write_in_full(fd, fanout, 0x100 * sizeof(unsigned int));
	
	/* hehehe, no crappy conversion for us HERE! */
	write_in_full(fd, body->buf, body->len);
	
	close(fd);
	
	return 0;
}

static int make_cache_index(const char *cache_sha1, struct strbuf *objects)
{
	struct strbuf buffer;
	int i, cache_index, cur;
	
	if (!idx_map)
		init_index();
	
	strbuf_init(&buffer, 0);
	if (idx_map) {
		strbuf_add(&buffer, idx_map + fanout[0], fanout[0x100] - fanout[0]);
	} else {
		/* not an update */
		memset(&idx_head, 0, sizeof(struct index_header));
		strcpy(idx_head.signature, "REVINDEX");
		idx_head.version = SUPPORTED_REVINDEX_VERSION;
		idx_head.ofs_objects = sizeof(struct index_header) + 0x100 * sizeof(unsigned int);
	}
	
	cache_index = idx_head.caches++;
	if (idx_head.caches >= idx_head.caches_buffer) {
		/* this whole dance is a bit useless currently, because we re-write everything regardless, 
		 * but later on we might decide to use a hashmap or something else which does not require 
		 * any particular on-disk format.  that would free us up to simply append new objects and 
		 * tweak the header accordingly */
		idx_head.caches_buffer += 20;
		idx_head.cache_sha1s = xrealloc(idx_head.cache_sha1s, idx_head.caches_buffer * 20);
		idx_head.ofs_objects += 20 * 20;
	}
	
	hashcpy(idx_head.cache_sha1s + cache_index * 20, cache_sha1);
	for (i = 0; i < objects->len; i += OE_SIZE) {
		struct index_entry index_entry, *entry;
		struct object_entry *object_entry = OE_CAST(objects->buf + i);
		
		if (object_entry->type != OBJ_COMMIT)
			continue;
		
		/* handle index duplication
		 * -> keep old copy unless new one is an end -- based on expected usage, older ones would be more 
		 * likely to lead to greater slice traversals than new ones
		 * todo: allow more intelligent overriding */
		entry = search_index(object_entry->sha1);
		if (entry && !object_entry->is_end)
			continue;
		else if (entry) /* mmm, pointer arithmetic... tasty */  /* (entry-index_map = offset, so cast is valid) */
			entry = IE_CAST(buffer.buf + (unsigned int)((unsigned char *)entry - idx_map) - fanout[0]);
		else
			entry = &index_entry;
		
		memset(entry, 0, sizeof(index_entry));
		hashcpy(entry->sha1, object_entry->sha1);
		entry->is_end = object_entry->is_end;
		entry->cache_index = cache_index;
		
		if (entry == &index_entry) {
			strbuf_add(&buffer, entry, sizeof(index_entry));
			idx_head.objects++;
		}
	}
	
	qsort(buffer.buf, buffer.len / IE_SIZE, IE_SIZE, index_sort_hash);
	
	/* generate fanout */
	cur = 0x00;
	for (i = 0; i < buffer.len; i += IE_SIZE) {
		struct index_entry *entry = IE_CAST(buffer.buf + i);
		
		while (cur <= entry->sha1[0])
			fanout[cur++] = i + idx_head.ofs_objects;
	}
	
	while (cur <= 0xff)
		fanout[cur++] = idx_head.ofs_objects + buffer.len;
	
	/* BOOM! */
	if (write_cache_index(&buffer))
		return -1;
	
	strbuf_release(&buffer);
	
	return 0;
}

static unsigned char *make_cache_pack(char *name, struct strbuf *objects)
{
	static unsigned char sha1[20];
	struct child_process pack_objects;
	const char *arg[5];
	char *path;
	struct packed_git *pack;
	int len, i, retval = -1;
	
	path = xstrdup(git_path("rev-cache/%s-0000000000000000000000000000000000000000.idx", name));
	len = strlen(path);
	path[len - 4 - 40 - 1] = 0;
	
	arg[0] = "pack-objects";
	arg[1] = path;
	arg[2] = 0;
	
	memset(&pack_objects, 0, sizeof(pack_objects));
	pack_objects.argv = arg;
	pack_objects.in = -1;
	pack_objects.out = -1;
	pack_objects.err = 0;
	pack_objects.git_cmd = 1;
	
	/* pack everything up! */
	if (start_command(&pack_objects) < 0)
		die("unable to start pack-objects");
	
	for (i = 0; i < objects->len; i += OE_SIZE) {
		struct object_entry *entry = OE_CAST(objects->buf + i);
		
		xwrite(pack_objects.in, sha1_to_hex(entry->sha1), 40);
		xwrite(pack_objects.in, "\n", 1);
	}
	
	close(pack_objects.in);
	xread(pack_objects.out, path + (len - 4 - 40), 40);
	if (get_sha1_hex(path + (len - 4 - 40), sha1))
		die("pack objects gave me a shitty sha1");
	
	if (finish_command(&pack_objects) < 0)
		die("pack objects didn't die of natural causes");
	
	/* now locate it */
	path[len - 4 - 40 - 1] = '-';
	pack = add_packed_git(path, len, 1);
	
	if (!pack)
		goto end;
	
	for (i = 0; i < objects->len; i += OE_SIZE) {
		struct object_entry *entry = OE_CAST(objects->buf + i);
		int off = find_pack_entry_one(entry->sha1, pack);
		
		if (!off)
			goto end;
		
		/* keep buffer write-ready */
		entry->ofs_pack = htonl(off);
	}
	
	retval = 0;
	
end:
	if (!retval)
		die("I can't traverse the pack index");
	
	free(path);
	
	return sha1;
}

static int get_index_head(unsigned char *map, int len, struct index_header *head, unsigned int *fanout)
{
	int i, index = sizeof(struct index_header);
	
	memcpy(head, map, sizeof(struct index_header));
	head->ofs_objects = ntohl(head->ofs_objects);
	head->objects = ntohl(head->objects);
	if (len < index + head->caches_buffer * 20 + (0x100) * sizeof(unsigned int))
		return -1;
	
	head->cache_sha1s = xmalloc(head->caches * 20);
	memcpy(head->cache_sha1s, map + index, head->caches * 20);
	index += head->caches_buffer * 20;
	
	memcpy(fanout, map + index, 0x100 * sizeof(unsigned int));
	for (i = 0; i <= 0xff; i++)
		fanout[i] = ntohl(fanout[i]);
	fanout[0x100] = len;
	
	return 0;
}

/* added in init_index */
static void cleanup_cache_slices(void)
{
	if (idx_map) {
		munmap(idx_map, idx_size);
		idx_map = 0;
	}
}

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

char *get_cache_slice(struct commit *commit)
{
	struct index_entry *ie;
	
	if (!idx_map)
		init_index();
	
	ie = search_index(commit->object.sha1);
	
	if (ie && ie->cache_index < idx_head.caches)
		return &idx_head.cache_sha1s[ie->cache_index * 20];
	return 0;
}

/* add end-commits from each cache slice (uninterestingness will be propogated) */
static void uninteresting_from_slices(struct rev_info *revs, unsigned char *which, int n)
{
	int i, index;
	struct commit *commit;
	
	if (!idx_map)
		init_index();
	if (!idx_map)
		return;
	
	/* haven't implemented which yet; no need really... */
	for (i = 0, index = idx_head.ofs_objects; i < idx_head.objects; i++, index += IE_SIZE) {
		struct index_entry *entry = IE_CAST(idx_map + index);
		
		if (!entry->is_end)
			continue;
		
		commit = lookup_commit(entry->sha1);
		if (!commit)
			continue;
		
		commit->object.flags |= UNINTERESTING;
		add_pending_object(revs, &commit->object, 0);
	}
	
}


/* porcelain */
static int handle_add(int argc, char *argv[]) /* args beyond this command */
{
	struct rev_info revs;
	char dostdin = 0;
	unsigned int flags = 0;
	int i;
	
	init_revisions(&revs, 0);
	
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--stdin"))
			dostdin = 1;
		else if (!strcmp(argv[i], "--fresh"))
			uninteresting_from_slices(&revs, 0, 0);
		else if (!strcmp(argv[i], "--not"))
			flags ^= UNINTERESTING;
		else if (!strcmp(argv[i], "--nopack"))
			dont_pack_it = 1;
		else
			handle_revision_arg(argv[i], &revs, flags, 1);
	}
	
	if (dostdin) {
		char line[1000];
		
		flags = 0;
		while (fgets(line, sizeof(line), stdin)) {
			int len = strlen(line);
			while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
				line[--len] = 0;
			
			if (!len)
				break;
			
			if (!strcmp(line, "--not"))
				flags ^= UNINTERESTING;
			else
				handle_revision_arg(line, &revs, flags, 1);
		}
	}
	
	return make_cache_slice(&revs, 0, 0);
}

static int handle_show(int argc, char *argv[])
{
	die("haven't implemented cache enumeration yet (try 'git-rev-cache help' to show usage)");
}

static int handle_rm(int argc, char *argv[])
{
	die("haven't implemented rm thingy yet");
}

static int handle_walk(int argc, char *argv[])
{
	struct commit *commit;
	unsigned char *sha1p;
	struct rev_info revs;
	struct commit_list *queue, *work, **qp;
	
	init_revisions(&revs, 0);
	
	if (!argc || handle_revision_arg(argv[0], &revs, 0, 1))
		die("I need a valid revision");
	
	commit = lookup_commit(revs.pending.objects[0].item->sha1);
	if (!commit)
		die("some random commit error");
	
	sha1p = get_cache_slice(commit);
	if (!sha1p)
		die("can't find cache slice");
	
	queue = work = 0;
	qp = &queue;
	printf("return value: %d\n", traverse_cache_slice(&revs, sha1p, commit, &qp, &work));
	
	printf("queue:\n");
	while ((commit = pop_commit(&queue)) != 0) {
		printf("%s\n", sha1_to_hex(commit->object.sha1));
	}
	
	printf("work:\n");
	while ((commit = pop_commit(&work)) != 0) {
		printf("%s\n", sha1_to_hex(commit->object.sha1));
	}
	
	return 0;
}

static int handle_help(void)
{
	char *usage = "\
half-assed usage guide:\n\
git-rev-cache COMMAND [options] [<commit-id>...]\n\
commands:\n\
 (none) - display caches.  passing a slice hash will display detailed\n\
          information about that cache slice\n\
 add    - add revisions to the cache.  reads commit ids from stdin, \n\
          formatted as: END END ... --not START START ...\n\
          options:\n\
           --fresh    exclude everything already in a cache slice\n\
           --nopack   don't generate a cache slice pack\n\
           --stdin    also read commit ids from stdin (same form as cmd)\n\
 rm     - delete a cache slice\n\
 walk   - walk a cache slice based on a given commit";
	
	puts(usage);
	
	return 0;
}

/*

usage:
git-rev-cache COMMAND [options] [<commit-id>...]
commands:
 (nothing)	- display caches.  passing a slice hash will display detailed
 			  information about that cache slice.
 add		- add revisions to the cache.  reads commit hashes from stdin, 
 			  formatted as: END END ... --not START START ...
 			  option --cmd allows reading from the command line (anything 
 			  beyond -- will be interpreted as a commit-id under this), 
 			  --fresh excludes anything already in a cache.
 rm			- delete a cache slice.  --all will remove everything, otherwise
 			  will read hashes from stdin.  --cmd will work as with add.
 walk		- walk a cache slice based on a given commit
   

*/

int main(int argc, char *argv[])
{
	char *arg;
	int r;
	
	if (argc > 1)
		arg = argv[1];
	else
		arg = "";
	
	argc -= 2;
	argv += 2;
	if (!strcmp(arg, "add"))
		r = handle_add(argc, argv);
	else if (!strcmp(arg, "rm"))
		r = handle_rm(argc, argv);
	else if (!strcmp(arg, "walk"))
		r = handle_walk(argc, argv);
	else if (!strcmp(arg, "help"))
		return handle_help();
	else
		r = handle_show(argc, argv);
	
	printf("final return value: %d\n", r);
	
	return 0;
}


#if 0
/*
experimentation...
*/
struct object_entry2 {
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
		if (hashcmp(object->sha1, end->sha1))
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
#endif

