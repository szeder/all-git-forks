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

static int get_index_head(unsigned char *map, int len, struct index_header *head, unsigned int *fanout)
{
	int i, index = sizeof(struct index_header);
	
	memcpy(head, map, sizeof(struct index_header));
	head->ofs_objects = ntohl(head->ofs_objects);
	head->objects = ntohl(head->objects);
	head->max_date = ntohl(head->max_date);
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


/* traversal */

static void handle_noncommit(struct rev_info *revs, struct object_entry *entry)
{
	struct object *obj = 0;
	
	switch (entry->type) {
	case OBJ_TREE : 
		if (revs->tree_objects)
			obj = (struct object *)lookup_tree(entry->sha1);
		break;
	case OBJ_BLOB : 
		if (revs->blob_objects)
			obj = (struct object *)lookup_blob(entry->sha1);
		break;
	case OBJ_TAG : 
		if (revs->tag_objects)
			obj = (struct object *)lookup_tag(entry->sha1);
		break;
	}
	
	if (!obj)
		return;
	
	obj->flags |= FACE_VALUE;
	add_pending_object(revs, obj, "");
}

static int setup_traversal(unsigned char *map, struct commit *commit, struct commit_list **work, int *ipath_nr, int *upath_nr)
{
	struct index_entry *iep;
	struct object_entry *oep;
	struct commit_list *prev, *wp, **wpp;
	int retval;
	
	iep = search_index(commit->object.sha1);
	oep = OE_CAST(map + ntohl(iep->pos));
	if (commit->object.flags & UNINTERESTING) {
		++*upath_nr;
		oep->uninteresting = 1;
	} else
		++*ipath_nr;
	oep->include = 1;
	retval = ntohl(iep->pos);
	
	/* include any others in the work array */
	prev = 0;
	wpp = work;
	wp = *work;
	while (wp) {
		struct object *obj = &wp->item->object;
		
		iep = search_index(obj->sha1);
		if (iep) {
			int t = ntohl(iep->pos);
			
			oep = OE_CAST(map + t);
			
			oep->include = 1;
			oep->uninteresting = !!(obj->flags & UNINTERESTING);
			if (t < retval)
				retval = t;
			
			/* remove from work list */
			pop_commit(wpp);
			wp = *wpp;
			if (prev)
				prev->next = wp;
		} else {
			prev = wp;
			wp = wp->next;
			wpp = &wp;
		}
		
		/* count even if not in slice so we can stop enumerating if possible */
		if (obj->flags & UNINTERESTING)
			++*upath_nr;
		else
			++*ipath_nr;
	}
	
	return retval;
}

#define IPATH				0x40
#define UPATH				0x80

#define GET_COUNT(x)		((x) & 0x3f)
#define SET_COUNT(x, s)		((x) = ((x) & ~0x3f) | ((s) & 0x3f))

static int traverse_cache_slice_1(struct rev_info *revs, struct cache_slice_header *head, unsigned char *map, 
	struct commit *commit, unsigned long *date_so_far, int *slop_so_far, struct commit_list ***queue, struct commit_list **work)
{
	struct commit_list *insert_cache = 0;
	struct commit **last_objects;
	unsigned long date = *date_so_far;
	int i, ipath_nr = 0, upath_nr = 0, total_path_nr = head->path_nr, slop = *slop_so_far, retval = -1;
	char consume_children = 0;
	unsigned char *paths;
	
	paths = xcalloc(total_path_nr, sizeof(unsigned short));
	last_objects = xcalloc(total_path_nr, sizeof(struct commit *));
	
	i = setup_traversal(map, commit, work, &ipath_nr, &upath_nr); /* printf("[%d] setup: %d; i: %d, u: %d\n", head->size, i, ipath_nr, upath_nr); */
	
	/* i already set */
	while (i < head->size) {
		struct object_entry *entry = OE_CAST(map + i);
		int path = ntohs(entry->path);
		struct commit *co;
		struct object *obj;
		int index = i;
		
		/* printf("%d/%d i:%d->%d (%d:%d:%d)\n", entry->type, OBJ_COMMIT, i, i + ACTUAL_OBJECT_ENTRY_SIZE(entry), 
			entry->merge_nr, entry->split_nr, entry->size_size); */
		i += ACTUAL_OBJECT_ENTRY_SIZE(entry);
		
		/* add extra objects if necessary */
		if (entry->type != OBJ_COMMIT) {
			if (consume_children)
				handle_noncommit(revs, entry);
			
			continue;
		} else
			consume_children = 0;
		
		/* printf("%s: %d/%d [%d:%d]\n", sha1_to_hex(entry->sha1), path, total_path_nr, entry->merge_nr, entry->split_nr); */
		if (path >= total_path_nr)
			goto end;
		
		/* printf("%s", sha1_to_hex(entry->sha1)); */
		
		/* in one of our branches? 
		 * uninteresting trumps interesting */
		if (entry->include)
			paths[path] |= entry->uninteresting ? UPATH : IPATH;
		else if (!paths[path])
			continue;
		
		/* date stuff */
		if (revs->max_age != -1 && ntohl(entry->date) < revs->max_age)
			paths[path] |= UPATH;
		
		/* printf("%s:paths[%d]: %2x\n", sha1_to_hex(entry->sha1), path, paths[path]); */
		if ((paths[path] & IPATH) && (paths[path] & UPATH)) {
			paths[path] = UPATH;
			ipath_nr--;
			
			/* mark edge */
			if (last_objects[path]) {
				if (!last_objects[path]->object.parsed) {
					/* don't want duplicates from our own topo relations */
					while (pop_commit(&last_objects[path]->parents)) ;
					parse_commit(last_objects[path]);
				}
				
				last_objects[path]->object.flags &= ~FACE_VALUE;
				last_objects[path] = 0;
			}
		}
		
		/* lookup object */
		entry->uninteresting = !!(paths[path] & UPATH);
		
		co = lookup_commit(entry->sha1);
		obj = &co->object;
		
		/* make topo relations */
		if (last_objects[path] && !last_objects[path]->object.parsed)
			commit_list_insert(co, &last_objects[path]->parents);
		
		/* first close paths */
		if (entry->split_nr) {
			int j, off = index + OE_SIZE + PATH_SIZE(entry->merge_nr);
			
			/* printf(" split: %d\n", entry->split_nr); */
			for (j = 0; j < entry->split_nr; j++) {
				unsigned short p = ntohs(*(unsigned short *)(map + off + PATH_SIZE(j)));
				
				if (p >= total_path_nr)
					goto end;
				
				/* printf("   paths[%d]: %2x\n", p, paths[p]); */
				
				/* boundary commit? */
				if ((paths[p] & IPATH) && entry->uninteresting) {
					if (last_objects[p]) {
						if (!last_objects[p]->object.parsed) {
							while (pop_commit(&last_objects[p]->parents)) ;
							parse_commit(last_objects[p]);
						}
						last_objects[p]->object.flags &= ~FACE_VALUE;
						last_objects[p] = 0;
					}
					obj->flags |= BOUNDARY;
				} else if (last_objects[p] && !last_objects[p]->object.parsed)
					commit_list_insert(co, &last_objects[p]->parents);
				
				/* can't close a merge path until all are parents have been encountered */
				if (GET_COUNT(paths[p])) {
					SET_COUNT(paths[p], GET_COUNT(paths[p]) - 1);
					
					if (GET_COUNT(paths[p]))
						continue;
				}
				
				/* printf("   closing"); */
				if (paths[p] & IPATH)
					ipath_nr--;
				else
					upath_nr--;
				
				paths[p] = 0;
				last_objects[p] = 0;
			}
		}
		
		/* we've been here already */
		if (obj->flags & SEEN && !entry->include) {
			if (entry->uninteresting && !(obj->flags & UNINTERESTING)) {
				obj->flags |= UNINTERESTING;
				mark_parents_uninteresting(co);
			}
			
			paths[path] = 0;
			/* we can ignore path_nrs -- if we've already been here then date is definitely < co->date */
			continue;
		}
		
		co->date = ntohl(entry->date);
		obj->flags |= SEEN | FACE_VALUE;
		
		if (entry->uninteresting)
			obj->flags |= UNINTERESTING;
		else
			date = co->date;
		
		/* we need to know what the edges are */
		last_objects[path] = co;
		
		/* add to list */
		if (!(revs->min_age != -1 && co->date > revs->min_age)) {
			
			if (!(obj->flags & UNINTERESTING) || revs->show_all) {
				if (entry->is_start)
					insert_by_date_cached(co, work, insert_cache, &insert_cache);
				else
					*queue = &commit_list_insert(co, *queue)->next;
				
				/* add children to list as well */
				if (obj->flags & UNINTERESTING)
					consume_children = 0;
				else 
					consume_children = 1;
			}
		}
		
		/* should we continue? */
		/* printf("slop: %d, i: %d, u: %d\n", slop, ipath_nr, upath_nr); */
		if (!slop) {
			if (!upath_nr)
				break;
			else {
				paths[path] = 0;
				upath_nr--;
			}
		} else if (!ipath_nr && co->date < date) {
			slop--;
			if (!slop && !revs->show_all)
				break;
		} else
			slop = SLOP;
		
		/* open parents */
		if (entry->merge_nr) {
			int j, off = index + OE_SIZE;
			char flag = entry->uninteresting ? UPATH : IPATH;
			
			/* printf(" merge: %d\n", entry->merge_nr); */
			for (j = 0; j < entry->merge_nr; j++) {
				unsigned short p = ntohs(*(unsigned short *)(map + off + PATH_SIZE(j)));
				
				if (p >= total_path_nr)
					goto end;
				
				/* printf("   paths[%d]: %2x\n", p, paths[p]); */
				if (paths[p] & flag)
					continue;
				
				if (flag == IPATH)
					ipath_nr++;
				else
					upath_nr++;
				
				paths[p] |= flag;
			}
			
			/* make sure we don't use this path before all our parents have had their say */
			SET_COUNT(paths[path], entry->merge_nr);
		} else if (entry->stop_me)
			SET_COUNT(paths[path], 1);
		
	}
	
	*date_so_far = date;
	*slop_so_far = slop;
	retval = 0;
	
end:
	free(paths);
	free(last_objects);
	
	return retval;
}

static int get_cache_slice_header(unsigned char *map, int len, struct cache_slice_header *head)
{
	int t;
	
	memcpy(head, map, sizeof(struct cache_slice_header));
	head->ofs_objects = ntohl(head->ofs_objects);
	head->objects = ntohl(head->objects);
	head->size = ntohl(head->size);
	head->path_nr = ntohs(head->path_nr);
	
	if (memcmp(head->signature, "REVCACHE", 8))
		return -1;
	if (head->version > SUPPORTED_REVCACHE_VERSION)
		return -2;
	
	t = sizeof(struct cache_slice_header);
	if (t != head->ofs_objects || t >= len)
		return -3;
	
	head->size = len;
	
	return 0;
}

int traverse_cache_slice(struct rev_info *revs, unsigned char *cache_sha1, 
	struct commit *commit, unsigned long *date_so_far, int *slop_so_far, 
	struct commit_list ***queue, struct commit_list **work)
{
	int fd = -1, retval = -3;
	struct stat fi;
	struct cache_slice_header head;
	unsigned char *map = MAP_FAILED;
	
	/* the index should've been loaded already to find cache_sha1, but it's good 
	 * to be absolutely sure... */
	if (!idx_map)
		init_index();
	if (!idx_map)
		return -1;
	
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
	
	retval = traverse_cache_slice_1(revs, &head, map, commit, date_so_far, slop_so_far, queue, work);
	
end:
	if (map != MAP_FAILED)
		munmap(map, fi.st_size);
	if (fd != -1)
		close(fd);
	
	return retval;
}



/* generation */

static int is_start_endpoint(struct commit *commit)
{
	struct commit_list *list = commit->parents;
	
	while (list) {
		if (!(list->item->object.flags & UNINTERESTING))
			return 0;
		
		list = list->next;
	}
	
	return 1;
}

/* ensures branch is self-contained: parents are either all interesting or all uninteresting */
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
		if (is_start_endpoint(item))
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


struct path_track {
	struct commit *commit;
	int path; /* for decrementing paths */
	struct strbuf path_str; /* for closing children */
	
	struct path_track *next, *prev;
};

static unsigned char *paths = 0;
static int path_nr = 1, path_sz = 0;

static struct path_track *children_to_close = 0, *paths_to_dec = 0;
static struct path_track *path_track_alloc = 0;

#define PATH_IN_USE			0x80 /* biggest bit we can get as a char */

static int get_new_path(void)
{
	int i;
	
	for (i = 1; i < path_nr; i++)
		if (!paths[i])
			break;
	
	if (i == path_nr) {
		if (path_nr >= path_sz) {
			path_sz += 50;
			paths = xrealloc(paths, path_sz);
			memset(paths + path_sz - 50, 0, 50);
		}
		path_nr++;
	}
	
	paths[i] = PATH_IN_USE;
	return i;
}

static void remove_path_track(struct path_track **ppt, char total_free)
{
	struct path_track *t = *ppt;
	
	if (t->next)
		t->next->prev = t->prev;
	if (t->prev)
		t->prev->next = t->next;
	
	t = t->next;
	
	if (total_free)
		free(*ppt);
	else {
		(*ppt)->next = path_track_alloc;
		path_track_alloc = *ppt;	
	}
	
	*ppt = t;
}

static struct path_track *find_path_track(struct path_track *head, struct commit *commit)
{
	while (head) {
		if (head->commit == commit)
			return head;
		head = head->next;
	}
	
	return 0;
}

static struct path_track *make_path_track(struct path_track **head, struct commit *commit)
{
	struct path_track *pt;
	
	if (path_track_alloc) {
		pt = path_track_alloc;
		path_track_alloc = pt->next;
	} else
		pt = xmalloc(sizeof(struct path_track));
	
	memset(pt, 0, sizeof(struct path_track));
	pt->commit = commit;
	
	pt->next = *head;
	if (*head)
		(*head)->prev = pt;
	*head = pt;
	
	return pt;
}

static void add_child_to_close(struct commit *commit, int path)
{
	struct path_track *pt = find_path_track(children_to_close, commit);
	unsigned short write_path;
	
	if (!pt) {
		pt = make_path_track(&children_to_close, commit);
		strbuf_init(&pt->path_str, 0);
	}
	
	write_path = htons((unsigned short)path);
	strbuf_add(&pt->path_str, &write_path, sizeof(unsigned short));
}

static void add_path_to_dec(struct commit *commit, int path)
{
	make_path_track(&paths_to_dec, commit);
	paths_to_dec->path = path;
}

static void handle_paths(struct commit *commit, struct object_entry *object, struct strbuf *merge_str, struct strbuf *split_str)
{
	int parent_nr, open_parent_nr, this_path;
	struct commit_list *list;
	struct commit *first_parent;
	struct path_track **ppt, *pt;
	
	/* we can only re-use a closed path once all it's children have been encountered, 
	 * as we need to keep track of commit boundaries */
	ppt = &paths_to_dec;
	pt = *ppt;
	while (pt) {
		if (pt->commit == commit) {
			if (paths[pt->path] != PATH_IN_USE)
				paths[pt->path]--;
			remove_path_track(ppt, 0);
			pt = *ppt;
		} else {
			pt = pt->next;
			ppt = &pt;
		}
	}
	
	/* the commit struct has no way of keeping track of children -- necessary for closing 
	 * unused paths and tracking path boundaries -- so we have to do it here */
	ppt = &children_to_close;
	pt = *ppt;
	while (pt) {
		if (pt->commit != commit) {
			pt = pt->next;
			ppt = &pt;
			continue;
		}
		
		object->split_nr = pt->path_str.len / sizeof(unsigned short);
		strbuf_add(split_str, pt->path_str.buf, pt->path_str.len);
		
		strbuf_release(&pt->path_str);
		remove_path_track(ppt, 0);
		break;
	}
	
	/* initialize our self! */
	if (!commit->indegree) {
		commit->indegree = get_new_path();
		object->is_end = 1;
	}
	
	this_path = commit->indegree;
	paths[this_path] = PATH_IN_USE;
	object->path = htons(this_path);
	
	/* count interesting parents */
	parent_nr = open_parent_nr = 0;
	first_parent = 0;
	for (list = commit->parents; list; list = list->next) {
		if (list->item->object.flags & UNINTERESTING) {
			object->is_start = 1;
			continue;
		}
		
		parent_nr++;
		if (!list->item->indegree)
			open_parent_nr++;
		if (!first_parent)
			first_parent = list->item;
	}
	
	if (!parent_nr)
		return;
	
	if (parent_nr == 1) {
		if (!open_parent_nr) {
			add_child_to_close(first_parent, this_path);
			add_path_to_dec(first_parent, this_path);
			
			object->stop_me = 1;
			paths[this_path] = 1;
		} else
			first_parent->indegree = this_path;
		return;
	}
	
	/* make merge list */
	object->merge_nr = parent_nr;
	paths[this_path] = parent_nr;
	
	for (list = commit->parents; list; list = list->next) {
		struct commit *p = list->item;
		unsigned short write_path;
		
		if (p->object.flags & UNINTERESTING)
			continue;
		
		/* unfortunately due to boundary tracking we can't re-use merge paths
		 * (unable to guarantee last parent path = this -> last won't always be able to 
		 * set this as a boundary object */
		if (!p->indegree)
			p->indegree = get_new_path();
		
		write_path = htons((unsigned short)p->indegree);
		strbuf_add(merge_str, &write_path, sizeof(unsigned short));
		
		/* make sure path is properly ended */
		add_child_to_close(p, this_path);
		add_path_to_dec(p, this_path);
	}
	
}


static void add_object_entry(const unsigned char *sha1, int type, struct object_entry *nothisone, 
	struct strbuf *merge_str, struct strbuf *split_str, struct strbuf *size_str)
{
	struct object_entry object;
	
	if (!nothisone) {
		memset(&object, 0, sizeof(object));
		hashcpy(object.sha1, sha1);
		object.type = type;
		
		if (merge_str)
			object.merge_nr = merge_str->len / sizeof(unsigned short);
		if (split_str)
			object.split_nr = split_str->len / sizeof(unsigned short);
		if (size_str)
			object.size_size = size_str->len;
		
		nothisone = &object;
	}
	
	strbuf_add(g_buffer, nothisone, sizeof(object));
	
	if (merge_str && merge_str->len)
		strbuf_add(g_buffer, merge_str->buf, merge_str->len);
	if (split_str && split_str->len)
		strbuf_add(g_buffer, split_str->buf, split_str->len);
	if (size_str && size_str->len)
		strbuf_add(g_buffer, size_str->buf, size_str->len);
}

static void tree_addremove(struct diff_options *options,
	int whatnow, unsigned mode,
	const unsigned char *sha1,
	const char *concatpath)
{
	unsigned char data[21];
	
	if (whatnow != '+')
		return;
	
	hashcpy(data, sha1);
	data[20] = !!S_ISDIR(mode);
	
	strbuf_add(g_buffer, data, 21);
}

static void tree_change(struct diff_options *options,
	unsigned old_mode, unsigned new_mode,
	const unsigned char *old_sha1,
	const unsigned char *new_sha1,
	const char *concatpath)
{
	unsigned char data[21];
	
	if (!hashcmp(old_sha1, new_sha1))
		return;
	
	hashcpy(data, new_sha1);
	data[20] = !!S_ISDIR(new_mode);
	
	strbuf_add(g_buffer, data, 21);
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
	unsigned char data[21];
	
	hashcpy(data, sha1);
	data[20] = !!S_ISDIR(mode);
	
	strbuf_add(g_buffer, data, 21);
	
	return 1;
}

/* {commit objects} \ {parent objects also in slice (ie. 'interesting')} */
static int add_unique_objects(struct commit *commit)
{
	struct commit_list *list;
	struct tree *first;
	struct strbuf os, us, *orig_buf;
	struct diff_options opts;
	int i, j;
	
	strbuf_init(&os, 0);
	strbuf_init(&us, 0);
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
	if (os.len)
		qsort(os.buf, os.len / 21, 21, (int (*)(const void *, const void *))hashcmp);
	
	g_buffer = &us;
	dump_tree(commit->tree, dump_tree_callback);
	qsort(us.buf, us.len / 21, 21, (int (*)(const void *, const void *))hashcmp);
	
	g_buffer = orig_buf;
	for (i = j = 0; i < us.len; i += 21) {
		while (j < os.len && hashcmp((const unsigned char *)(os.buf + j), (const unsigned char *)(us.buf + i)) < 0)
			j += 21;
		
		if (j < os.len && !hashcmp((const unsigned char *)(os.buf + j), (const unsigned char *)(us.buf + i)))
			continue;
		
		/* todo: get size */
		add_object_entry((const unsigned char *)(us.buf + i), us.buf[i + 20] ? OBJ_TREE : OBJ_BLOB, 0, 0, 0, 0);
	}
	
	strbuf_release(&us);
	strbuf_release(&os);
	
	return 0;
}

static int make_cache_index(int fd, unsigned char *cache_sha1, unsigned int ofs_objects, unsigned int size, unsigned long max_date);

int make_cache_slice(struct rev_info *revs, struct commit_list **ends, struct commit_list **starts, char do_legs)
{
	struct commit_list *list;
	struct rev_info therevs;
	struct strbuf buffer, endlist, startlist;
	struct cache_slice_header head;
	struct commit *commit;
	unsigned char sha1[20];
	struct strbuf merge_paths, split_paths;
	int object_nr, total_sz, fd;
	unsigned long max_date;
	char file[PATH_MAX], *newfile;
	git_SHA_CTX ctx;
	
	strcpy(file, git_path("rev-cache/XXXXXX"));
	fd = xmkstemp(file);
	
	strbuf_init(&buffer, 0);
	strbuf_init(&endlist, 0);
	strbuf_init(&startlist, 0);
	strbuf_init(&merge_paths, 0);
	strbuf_init(&split_paths, 0);
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
	
	/* write head placeholder */
	memset(&head, 0, sizeof(head));
	head.ofs_objects = htonl(sizeof(head));
	xwrite(fd, &head, sizeof(head));
	
	/* init revisions! */
	revs->tree_objects = 1;
	revs->blob_objects = 1;
	revs->topo_order = 1;
	revs->lifo = 1;
	revs->beyond_hash = 1; /* do _not_ want ourselves caching */
	
	setup_revisions(0, 0, revs, 0);
	if (prepare_revision_walk(revs))
		die("died preparing revision walk");
	
	if (do_legs)
		make_legs(revs);
	
	object_nr = total_sz = 0;
	max_date = 0;
	while ((commit = get_revision(revs)) != 0) {
		struct object_entry object;
		
		strbuf_setlen(&merge_paths, 0);
		strbuf_setlen(&split_paths, 0);
		
		memset(&object, 0, sizeof(object));
		object.type = OBJ_COMMIT;
		object.date = htonl(commit->date);
		hashcpy(object.sha1, commit->object.sha1);
		
		handle_paths(commit, &object, &merge_paths, &split_paths);
		
		if (object.is_start)
			strbuf_add(&startlist, object.sha1, 20);
		if (object.is_end)
			strbuf_add(&endlist, object.sha1, 20);
		
		commit->indegree = 0;
		if (commit->date > max_date)
			max_date = commit->date;
		
		/* printf("%s [%d] [%d-%d:%d-%d]\n", sha1_to_hex(object.sha1), object.is_start, 
			object.merge_nr, merge_paths.len, object.split_nr, split_paths.len); */
		/* todo: get size for this and tree */
		add_object_entry(0, 0, &object, &merge_paths, &split_paths, 0);
		object_nr++;
		
		if (!(commit->object.flags & TREESAME)) {
			/* add all unique children for this commit */
			add_object_entry(commit->tree->object.sha1, OBJ_TREE, 0, 0, 0, 0);
			object_nr++;
			
			if (!object.is_start)
				object_nr += add_unique_objects(commit);
		}
		
		/* print every ~1MB or so */
		if (buffer.len > 1000000) {
			write_in_full(fd, buffer.buf, buffer.len);
			total_sz += buffer.len;
			
			strbuf_setlen(&buffer, 0);
		}
	}
	
	if (buffer.len) {
		write_in_full(fd, buffer.buf, buffer.len);
		total_sz += buffer.len;
	}
	
	/* now actually initialize header */
	strcpy(head.signature, "REVCACHE");
	head.version = SUPPORTED_REVCACHE_VERSION;
	
	head.objects = htonl(object_nr);
	head.size = htonl(ntohl(head.ofs_objects) + total_sz);
	head.path_nr = htons(path_nr); printf("paths: %d\n", path_nr);
	
	lseek(fd, 0, SEEK_SET);
	xwrite(fd, &head, sizeof(head));
	
	/* go ahead a free some stuff... */
	strbuf_release(&buffer);
	strbuf_release(&merge_paths);
	strbuf_release(&split_paths);
	if (path_sz)
		free(paths);
	while (path_track_alloc)
		remove_path_track(&path_track_alloc, 1);
	
	/* the meaning of the hash name is more or less irrelevant, it's the uniqueness that matters */
	strbuf_add(&startlist, endlist.buf, endlist.len);
	git_SHA1_Init(&ctx);
	git_SHA1_Update(&ctx, startlist.buf, startlist.len);
	git_SHA1_Final(sha1, &ctx);
	
	if (make_cache_index(fd, sha1, ntohl(head.ofs_objects), ntohl(head.size), max_date) < 0)
		die("can't update index");
	
	close(fd);
	
	newfile = git_path("rev-cache/%s", sha1_to_hex(sha1));
	/* unlink/link stuff? */
	rename(file, newfile);
	
	strbuf_release(&startlist);
	strbuf_release(&endlist);
	
	return 0;
}


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
	whead.max_date = htonl(whead.max_date);
	write(fd, &whead, sizeof(struct index_header));
	write_in_full(fd, idx_head.cache_sha1s, idx_head.caches_buffer * 20);
	
	for (i = 0; i <= 0xff; i++)
		fanout[i] = htonl(fanout[i]);
	write_in_full(fd, fanout, 0x100 * sizeof(unsigned int));
	
	write_in_full(fd, body->buf, body->len);
	
	close(fd);
	
	return 0;
}

static int make_cache_index(int fd, unsigned char *cache_sha1, unsigned int ofs_objects, unsigned int size, unsigned long max_date)
{
	struct strbuf buffer;
	int i, cache_index, cur;
	unsigned char *map;
	
	if (!idx_map)
		init_index();
	
	lseek(fd, 0, SEEK_SET);
	map = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED)
		return -1;
	
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
	i = ofs_objects;
	while (i < size) {
		struct index_entry index_entry, *entry;
		struct object_entry *object_entry = OE_CAST(map + i);
		int pos = i;
		
		i += ACTUAL_OBJECT_ENTRY_SIZE(object_entry);
		
		if (object_entry->type != OBJ_COMMIT)
			continue;
		
		/* handle index duplication
		 * -> keep old copy unless new one is an end -- based on expected usage, older ones will be more 
		 * likely to lead to greater slice traversals than new ones
		 * todo: allow more intelligent overriding */
		if (ntohl(object_entry->date) > idx_head.max_date)
			entry = 0;
		else
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
		entry->pos = htonl(pos);
		
		if (entry == &index_entry) {
			strbuf_add(&buffer, entry, sizeof(index_entry));
			idx_head.objects++;
		}
		
	}
	
	idx_head.max_date = max_date;
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
	
	munmap(map, size);
	strbuf_release(&buffer);
	
	return 0;
}


/* add end-commits from each cache slice (uninterestingness will be propogated) */
void uninteresting_from_slices(struct rev_info *revs, unsigned char *which, int n)
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





#if 0

/* porcelain for rev-cache.c */
static int handle_add(int argc, const char *argv[]) /* args beyond this command */
{
	struct rev_info revs;
	char dostdin = 0, dont_pack_it = 0, do_legs = 0;
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
		else if (!strcmp(argv[i], "--legs"))
			do_legs = 1;
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
	
	return make_cache_slice(&revs, 0, 0, do_legs);
}

static int handle_show(int argc, const char *argv[])
{
	die("haven't implemented cache enumeration yet (try 'git-rev-cache help' to show usage)");
}

static int handle_rm(int argc, const char *argv[])
{
	die("haven't implemented rm thingy yet");
}

static int handle_walk(int argc, const char *argv[])
{
	struct commit *commit;
	struct rev_info revs;
	struct commit_list *queue, *work, **qp;
	unsigned char *sha1p, *sha1pt;
	unsigned long date = 0;
	unsigned int flags = 0;
	int slop = 5, i;
	
	init_revisions(&revs, 0);
	
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--not"))
			flags ^= UNINTERESTING;
		else
			handle_revision_arg(argv[i], &revs, flags, 1);
	}
	
	work = 0;
	sha1p = 0;
	for (i = 0; i < revs.pending.nr; i++) {
		commit = lookup_commit(revs.pending.objects[i].item->sha1);
		
		sha1pt = get_cache_slice(commit->object.sha1);
		if (!sha1pt)
			die("%s: not in a cache slice", commit->object.sha1);
		
		if (!i)
			sha1p = sha1pt;
		else if (sha1p != sha1pt)
			die("walking porcelain is /per/ cache slice; commits cannot be spread out amoung several");
		
		insert_by_date(commit, &work);
	}
	
	if (!sha1p)
		die("nothing to traverse!");
	
	queue = 0;
	qp = &queue;
	commit = pop_commit(&work);
	printf("return value: %d\n", traverse_cache_slice(&revs, sha1p, commit, &date, &slop, &qp, &work));
	
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
           --legs     ensure branch is entirely self-contained\n\
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

int main(int argc, const char *argv[])
{
	const char *arg;
	int r;
	
	git_config(git_default_config, NULL);
	
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

#endif