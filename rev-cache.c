#include <assert.h>
#include "cache.h"
#include "object.h"
#include "commit.h"
#include "tree.h"
#include "tree-walk.h"
#include "diff.h"
#include "revision.h"

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
};

/* the size field might be screwy, but we need entries to have 
a fixed size; we could use two ints... */
struct object_entry {
	unsigned type : 3;
	unsigned is_start : 1;
	unsigned is_end : 1;
	unsigned flags : 3; /* for later */
	unsigned char sha1[20];
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

#define SUPPORTED_REVCACHE_VERSION 		1
#define SUPPORTED_REVINDEX_VERSION		1

/* yes it makes the ordering a bit weird, but it's simpler */
#define SET_BIT(b, i)	((b)[(i) >> 3] |= 1 << ((i) & 0x7))
#define GET_BIT(b, i)	!!((b)[(i) >> 3] &  1 << ((i) & 0x7))
#define BITMAP_SIZE(os)	((os) / 8 + 1)

#define OE_SIZE	sizeof(struct object_entry)
#define IE_SIZE	sizeof(struct index_entry)

int make_cache_index(const char *, struct strbuf *);
static int init_index(void);
static void cleanup_cache_slices(void);
static struct index_entry *search_index(unsigned char *);

/* this will happen so rarely with so few that it really dosn't matter how we do it */
static int in_sha1_list(const unsigned char *list, int n, const unsigned char *sha1)
{
	int i;
	
	for (i = 0; i < n; i++) {
		if (list[i * 20] != sha1[0] || memcmp(&list[i * 20], sha1, 20))
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
		object = (struct object_entry *)(map + index);
		
		if (object->type != OBJ_COMMIT)
			continue;
		if (memcmp(object->sha1, cur_commit->object.sha1, 20)) 
			continue;
		
		/* printf("%d setting %s\n", i >> 3, sha1_to_hex(object->sha1)); */
		SET_BIT(bitmap, i);
		cur_commit->object.flags &= ~HEARD; /* we're very tidy! */
		cur_commit = pop_commit(&list);
	}
	
	/* this should never happen! */
	assert(!cur_commit);
	
	memcpy(bitmap_entry->sha1, end->object.sha1, 20);
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
		
		if (!memcmp(be->sha1, end->object.sha1, 20))
			break;
		
		index += sizeof(struct bitmap_entry) + be->z_size;
	}
	
	if (index >= head->size - sizeof(struct bitmap_entry))
		return 1;
	
	memcpy(bitmap->sha1, be->sha1, 20);
	bitmap->z_size = be->z_size;
	
	bitmap->bitmap = xcalloc(BITMAP_SIZE(head->objects), 1);
	memcpy(bitmap->bitmap, map + index + sizeof(struct bitmap_entry), bitmap->z_size);
	if (deflate_bitmap(bitmap->bitmap, bitmap->z_size) != BITMAP_SIZE(head->objects))
		return -1;
	
	return 0;
}

static int get_cache_slice_header(unsigned char *map, int len, struct cache_slice_header *head)
{
	int t;
	
	memcpy(head, map, sizeof(struct cache_slice_header));
	head->start_sha1s = 0;
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
	struct commit_list ***queue, struct commit_list **work)
{
	struct commit_list *q = 0, *w = 0;
	struct commit_list **qp = &q, **wp = &w;
	int i, index, retval = -2;
	char use_objects = 1, consuming_children = 0; /* that's right, this function is the evil twin */
	unsigned char *anti_bitmap = 0;
	struct bitmap_entry be;
	
	for (i = 0, index = head->ofs_objects; i < head->objects; i++, index += OE_SIZE) {
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
		
		entry = (struct object_entry *)(map + index);
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
					if (be.bitmap[j])
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
				if (!(object->flags & UNINTERESTING))
					consuming_children = use_objects = 1;
			}
		} else if (!(object->flags & UNINTERESTING) || revs->show_all) {
			if (!(object->flags & SEEN)) {
				object->flags |= SEEN;
				qp = &commit_list_insert(co, qp)->next;
				
				if (!(object->flags & UNINTERESTING))
					consuming_children = use_objects = 1;
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
	unsigned char *bitmaps; /* what we've done */
	unsigned char *map;
	int size;
	
	struct cache_slice_header head;
	struct rev_cache *next;
};

/* revs, which cache, object sha1, queue list, work list */
int traverse_cache_slice(struct rev_info *revs, unsigned char *cache_sha1, 
	struct commit *commit, struct commit_list ***queue, struct commit_list **work)
{
	int fd = -1, made, retval = -1;
	struct stat fi;
	struct bitmap_entry bitmap;
	struct cache_slice_header head;
	unsigned char *map = MAP_FAILED;
	
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
	
	made = get_bitmap(&head, map, commit, &bitmap);
	if (made < 0)
		goto end;
	else if (made > 0) {
		make_bitmap(&head, map, commit, &bitmap);
		bitmap.z_size = compress_bitmap(bitmap.bitmap, BITMAP_SIZE(head.objects));
		
		/* yes we really are writing the useless pointer address too */
		lseek(fd, fi.st_size, SEEK_SET);
		write(fd, &bitmap, sizeof(struct bitmap_entry));
		write(fd, bitmap.bitmap, bitmap.z_size);
		
		deflate_bitmap(bitmap.bitmap, bitmap.z_size);
	}
	
	retval = traverse_cache_slice_1(revs, &head, map, bitmap.bitmap, commit, queue, work);
	
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
	
	list = commit->parents;
	while (list) {
		struct object *p = &list->item->object;
		
		if (!(p->flags & UNINTERESTING))
			return 0;
	}
	
	return 1;
}

static void add_object_entry(const char *sha1, int type, struct object_entry *nothisone)
{
	struct object_entry object;
	
	if (!nothisone) {
		memset(&object, 0, sizeof(object));
		object.type = type;
		memcpy(object.sha1, sha1, 20);
		
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
	return hashcmp(((struct object_entry *)a)->sha1, ((struct object_entry *)b)->sha1);
}

static int cache_sort_type(const void *a, const void *b)
{
	struct object_entry *entry1, *entry2;
	
	entry1 = (struct object_entry *)a;
	entry2 = (struct object_entry *)b;
	
	if (entry1->type == entry2->type) 
		return 0;
	
	return entry1->type < entry2->type ? 1 : -1;
}

static int write_cache_slice(char *name, struct cache_slice_header *head, struct strbuf *body)
{
	int fd;
	
	fd = open(git_path("rev-cache/%s", name), O_CREAT | O_WRONLY, 0666);
	if (fd < 0)
		return -1;
	
	write(fd, head, sizeof(struct cache_slice_header));
	write(fd, head->start_sha1s, head->starts * 20);
	write(fd, head->end_sha1s, head->ends * 20);
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
		
		add_object_entry(0, 0, (struct object_entry *)(set->buf + i));
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

#define NOT_END			TMP_MARK

int make_cache_slice(struct rev_info *revs, struct commit_list **ends, struct commit_list **starts)
{
	struct commit_list *list;
	struct rev_info therevs;
	struct strbuf buffer, endlist, startlist;
	struct cache_slice_header head;
	struct commit *commit;
	unsigned char sha1[20];
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
	
	/* using the very system we're optimizing may seem a bit weird, but as we 
	 * shouldn't ever be making cache slices _within_ a traversal we should be ok */
	while ((commit = get_revision(revs)) != 0) {
		struct object_entry object;
		
		memset(&object, 0, sizeof(object));
		object.type = OBJ_COMMIT;
		memcpy(object.sha1, commit->object.sha1, 20);
		
		/* determine if this is an endpoint: 
		 * if all parents are uninteresting -> start
		 * if this isn't a parent from a SEEN -> end */
		if (is_start(commit)) {
			object.is_start = 1;
			strbuf_add(&startlist, commit->object.sha1, 20);
		}
		
		if (!(commit->object.flags & NOT_END)) {
			object.is_end = 1;
			strbuf_add(&endlist, commit->object.sha1, 20);
		} else
			commit->object.flags &= ~NOT_END;
		
		for (list = commit->parents; list; list = list->next) {
			if (list->item->object.flags & UNINTERESTING)
				continue;
			list->item->object.flags |= NOT_END;
		}
		
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
	head.start_sha1s = startlist.buf;
	head.ends = endlist.len / 20;
	head.end_sha1s = endlist.buf;
	
	head.ofs_objects = sizeof(head) + startlist.len + endlist.len;
	head.ofs_bitmaps = head.ofs_objects + buffer.len;
	head.objects = buffer.len / OE_SIZE;
	head.size = head.ofs_bitmaps;
	
	/* the meaning of the hash name is more or less irrelevant, it's the uniqueness that matters */
	strbuf_add(&startlist, strbuf_detach(&endlist, 0), head.ends * 20);
	git_SHA1_Init(&ctx);
	git_SHA1_Update(&ctx, startlist.buf, startlist.len);
	git_SHA1_Final(sha1, &ctx);
	
	if (write_cache_slice(sha1_to_hex(sha1), &head, &buffer) < 0)
		die("write failed");
	
	if (make_cache_index(sha1, &buffer) < 0)
		die("can't update index");
	
	strbuf_release(&buffer);
	strbuf_release(&startlist);
	strbuf_release(&endlist);
	
	return 0;
}

/* todo: add a garbage cleaner to weed out unused stuff from slices and index */

static int index_sort_hash(const void *a, const void *b)
{
	return hashcmp(((struct index_entry *)a)->sha1, ((struct index_entry *)b)->sha1);
}

/* todo: handle concurrency issues */
static int write_cache_index(struct strbuf *body)
{
	int fd;
	
	cleanup_cache_slices();
	
	fd = open(git_path("rev-cache/index"), O_CREAT | O_WRONLY, 0666);
	if (fd < 0)
		return -1;
	
	write(fd, &idx_head, sizeof(struct index_header));
	write(fd, idx_head.cache_sha1s, idx_head.caches_buffer * 20);
	write(fd, fanout, 0x100 * sizeof(unsigned int));
	write_in_full(fd, body->buf, body->len);
	
	close(fd);
	
	return 0;
}

int make_cache_index(const char *cache_sha1, struct strbuf *objects)
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
	
	idx_head.objects += objects->len / OE_SIZE;
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
	
	memcpy(idx_head.cache_sha1s + cache_index * 20, cache_sha1, 20);
	for (i = 0; i < objects->len; i += IE_SIZE) {
		struct index_entry index_entry, *entry;
		struct object_entry *object_entry = (struct object_entry *)(objects->buf + i);
		
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
			entry = (struct index_entry *)(buffer.buf + (unsigned int)((unsigned char *)entry - idx_map) - fanout[0]);
		else
			entry = &index_entry;
		
		memset(entry, 0, sizeof(index_entry));
		memcpy(entry->sha1, object_entry->sha1, 20);
		entry->is_end = object_entry->is_end;
		entry->cache_index = cache_index;
		
		if (entry == &index_entry)
			strbuf_add(&buffer, entry, sizeof(index_entry));
	}
	
	qsort(buffer.buf, buffer.len / IE_SIZE, IE_SIZE, index_sort_hash);
	
	/* generate fanout */
	cur = 0x00;
	for (i = 0; i < buffer.len; i += IE_SIZE) {
		struct index_entry *entry = (struct index_entry *)(buffer.buf + i);
		
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

static int get_index_head(unsigned char *map, int len, struct index_header *head, unsigned int *fanout)
{
	int index = sizeof(struct index_header);
	
	memcpy(head, map, sizeof(struct index_header));
	if (len < index + head->caches_buffer * 20 + (0x100) * sizeof(unsigned int))
		return -1;
	
	head->cache_sha1s = xmalloc(head->caches * 20);
	memcpy(head->cache_sha1s, map + index, head->caches * 20);
	index += head->caches_buffer * 20;
	
	memcpy(fanout, map + index, 0x100 * sizeof(unsigned int));
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
	
	/* todo: store mapping/fanout for re-use */
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
		i = (endi + starti + 1) / 2;
		ie = (struct index_entry *)(idx_map + start + i * IE_SIZE);
		r = hashcmp(sha1, ie->sha1);
		
		if (r) {
	 		if (starti == endi)
				break;
			/* if (i == starti) {
				starti++;
				continue;
			} */
			
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
		return &idx_head.cache_sha1s[ie->cache_index];
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
		struct index_entry *entry = (struct index_entry *)(idx_map + index);
		
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
	die("haven't implemented cache enumeration yet");
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
	char *usage = 
"half-assed usage guide:\n\
git-rev-cache COMMAND [options] [<commit-id>...]\n\
commands:\n\
 (nothing)	- display caches.  passing a slice hash will display detailed\n\
 			  information about that cache slice.\n\
 add		- add revisions to the cache.  reads commit hashes from stdin, \n\
 			  formatted as: END END ... --not START START ...\n\
 			  option --cmd allows reading from the command line (anything \n\
 			  beyond -- will be interpreted as a commit-id under this), \n\
 			  --fresh excludes anything already in a cache.\n\
 rm			- delete a cache slice.  --all will remove everything, otherwise\n\
 			  will read hashes from stdin.  --cmd will work as with add.\n\
 walk		- walk a cache slice based on a given commit";
	
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
#endif

