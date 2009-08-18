#include "cache.h"
#include "object.h"
#include "commit.h"
#include "tree.h"
#include "tree-walk.h"
#include "blob.h"
#include "tag.h"
#include "diff.h"
#include "revision.h"
#include "rev-cache.h"
#include "run-command.h"

/* list resembles pack index format */
static uint32_t fanout[0xff + 2];

static unsigned char *idx_map;
static int idx_size;
static struct rc_index_header idx_head;
static unsigned char *idx_caches;
static char no_idx;

static struct strbuf *acc_buffer;

#define SLOP			5

/* initialization */

struct rc_index_entry *from_disked_rc_index_entry(struct rc_index_entry_ondisk *src, struct rc_index_entry *dst)
{
	static struct rc_index_entry entry[4];
	static int cur;

	if (!dst)
		dst = &entry[cur++ & 0x3];

	dst->sha1 = src->sha1;
	dst->is_start = !!(src->flags & 0x80);
	dst->cache_index = src->flags & 0x7f;
	dst->pos = ntohl(src->pos);

	return dst;
}

struct rc_index_entry_ondisk *to_disked_rc_index_entry(struct rc_index_entry *src, struct rc_index_entry_ondisk *dst)
{
	static struct rc_index_entry_ondisk entry[4];
	static int cur;

	if (!dst)
		dst = &entry[cur++ & 0x3];

	if (dst->sha1 != src->sha1)
		hashcpy(dst->sha1, src->sha1);
	dst->flags = (unsigned char)src->is_start << 7 | (unsigned char)src->cache_index;
	dst->pos = htonl(src->pos);

	return dst;
}

struct rc_object_entry *from_disked_rc_object_entry(struct rc_object_entry_ondisk *src, struct rc_object_entry *dst)
{
	static struct rc_object_entry entry[4];
	static int cur;

	if (!dst)
		dst = &entry[cur++ & 0x3];

	dst->type = src->flags >> 5;
	dst->is_end = !!(src->flags & 0x10);
	dst->is_start = !!(src->flags & 0x08);
	dst->uninteresting = !!(src->flags & 0x04);
	dst->include = !!(src->flags & 0x02);
	dst->flag = !!(src->flags & 0x01);

	dst->sha1 = src->sha1;
	dst->merge_nr = src->merge_nr;
	dst->split_nr = src->split_nr;

	dst->size_size = src->sizes >> 5;
	dst->padding = src->sizes & 0x1f;

	dst->date = ntohl(src->date);
	dst->path = ntohs(src->path);

	return dst;
}

struct rc_object_entry_ondisk *to_disked_rc_object_entry(struct rc_object_entry *src, struct rc_object_entry_ondisk *dst)
{
	static struct rc_object_entry_ondisk entry[4];
	static int cur;

	if (!dst)
		dst = &entry[cur++ & 0x3];

	dst->flags  = (unsigned char)src->type << 5;
	dst->flags |= (unsigned char)src->is_end << 4;
	dst->flags |= (unsigned char)src->is_start << 3;
	dst->flags |= (unsigned char)src->uninteresting << 2;
	dst->flags |= (unsigned char)src->include << 1;
	dst->flags |= (unsigned char)src->flag;

	if (dst->sha1 != src->sha1)
		hashcpy(dst->sha1, src->sha1);
	dst->merge_nr = src->merge_nr;
	dst->split_nr = src->split_nr;

	dst->sizes  = (unsigned char)src->size_size << 5;
	dst->sizes |= (unsigned char)src->padding;

	dst->date = htonl(src->date);
	dst->path = htons(src->path);

	return dst;
}

static int get_index_head(unsigned char *map, int len, struct rc_index_header *head, uint32_t *fanout, unsigned char **caches)
{
	struct rc_index_header whead;
	int i, index = sizeof(struct rc_index_header);

	memcpy(&whead, map, sizeof(struct rc_index_header));
	if (memcmp(whead.signature, "REVINDEX", 8) || whead.version != SUPPORTED_REVINDEX_VERSION)
		return -1;

	memcpy(head->signature, "REVINDEX", 8);
	head->version = whead.version;
	head->ofs_objects = ntohl(whead.ofs_objects);
	head->object_nr = ntohl(whead.object_nr);
	head->cache_nr = whead.cache_nr;
	head->max_date = ntohl(whead.max_date);

	if (len < index + head->cache_nr * 20 + 0x100 * sizeof(uint32_t))
		return -2;

	*caches = xmalloc(head->cache_nr * 20);
	memcpy(*caches, map + index, head->cache_nr * 20);
	index += head->cache_nr * 20;

	memcpy(fanout, map + index, 0x100 * sizeof(uint32_t));
	for (i = 0; i <= 0xff; i++)
		fanout[i] = ntohl(fanout[i]);
	fanout[0x100] = len;

	return 0;
}

/* added in init_index */
static void cleanup_cache_slices(void)
{
	if (idx_map) {
		free(idx_caches);
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
	if (fi.st_size < sizeof(struct rc_index_header))
		goto end;

	idx_size = fi.st_size;
	idx_map = xmmap(0, idx_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (idx_map == MAP_FAILED)
		goto end;
	if (get_index_head(idx_map, fi.st_size, &idx_head, fanout, &idx_caches))
		goto end;

	atexit(cleanup_cache_slices);

	return 0;

end:
	idx_map = 0;
	no_idx = 1;
	return -1;
}

/* this assumes index is already loaded */
static struct rc_index_entry_ondisk *search_index_1(unsigned char *sha1)
{
	int start, end, starti, endi, i, len, r;
	struct rc_index_entry_ondisk *ie;

	if (!idx_map)
		return 0;

	/* binary search */
	start = fanout[(int)sha1[0]];
	end = fanout[(int)sha1[0] + 1];
	len = (end - start) / sizeof(struct rc_index_entry_ondisk);
	if (!len || len * sizeof(struct rc_index_entry_ondisk) != end - start)
		return 0;

	starti = 0;
	endi = len - 1;
	for (;;) {
		i = (endi + starti) / 2;
		ie = (struct rc_index_entry_ondisk *)(idx_map + start + i * sizeof(struct rc_index_entry_ondisk));
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

static struct rc_index_entry *search_index(unsigned char *sha1)
{
	struct rc_index_entry_ondisk *ied = search_index_1(sha1);

	if (ied)
		return from_disked_rc_index_entry(ied, 0);

	return 0;
}

unsigned char *get_cache_slice(struct commit *commit)
{
	struct rc_index_entry *ie;

	if (!idx_map) {
		if (no_idx)
			return 0;
		init_index();
	}

	if (commit->date > idx_head.max_date)
		return 0;

	ie = search_index(commit->object.sha1);
	if (ie && ie->cache_index < idx_head.cache_nr)
		return idx_caches + ie->cache_index * 20;

	return 0;
}


/* traversal */

static int setup_traversal(struct rc_slice_header *head, unsigned char *map, struct commit *commit, struct commit_list **work)
{
	struct rc_index_entry *iep;
	struct rc_object_entry *oep;
	struct commit_list *prev, *wp, **wpp;
	int retval;

	iep = search_index(commit->object.sha1), 0;
	oep = RC_OBTAIN_OBJECT_ENTRY(map + iep->pos);

	/* the .uniniteresting bit isn't strictly necessary, as we check the object during traversal as well,
	 * but we might as well initialize it while we're at it */
	oep->include = 1;
	oep->uninteresting = !!(commit->object.flags & UNINTERESTING);
	to_disked_rc_object_entry(oep, (struct rc_object_entry_ondisk *)(map + iep->pos));
	retval = iep->pos;

	/* include any others in the work array */
	prev = 0;
	wpp = work;
	wp = *work;
	while (wp) {
		struct object *obj = &wp->item->object;
		struct commit *co;

		/* is this in our cache slice? */
		iep = search_index(obj->sha1);
		if (!iep || hashcmp(idx_caches + iep->cache_index * 20, head->sha1)) {
			prev = wp;
			wp = wp->next;
			wpp = &wp;
			continue;
		}

		if (iep->pos < retval)
			retval = iep->pos;

		oep = RC_OBTAIN_OBJECT_ENTRY(map + iep->pos);

		/* mark this for later */
		oep->include = 1;
		oep->uninteresting = !!(obj->flags & UNINTERESTING);
		to_disked_rc_object_entry(oep, (struct rc_object_entry_ondisk *)(map + iep->pos));

		/* remove from work list */
		co = pop_commit(wpp);
		wp = *wpp;
		if (prev)
			prev->next = wp;
	}

	return retval;
}

#define IPATH				0x40
#define UPATH				0x80

#define GET_COUNT(x)		((x) & 0x3f)
#define SET_COUNT(x, s)		((x) = ((x) & ~0x3f) | ((s) & 0x3f))

static int traverse_cache_slice_1(struct rc_slice_header *head, unsigned char *map,
	struct rev_info *revs, struct commit *commit,
	unsigned long *date_so_far, int *slop_so_far,
	struct commit_list ***queue, struct commit_list **work)
{
	struct commit_list *insert_cache = 0;
	struct commit **last_objects, *co;
	int i, total_path_nr = head->path_nr, retval = -1;
	char consume_children = 0;
	unsigned char *paths;

	i = setup_traversal(head, map, commit, work);
	if (i < 0)
		return -1;

	paths = xcalloc(total_path_nr, sizeof(uint16_t));
	last_objects = xcalloc(total_path_nr, sizeof(struct commit *));

	/* i already set */
	while (i < head->size) {
		struct rc_object_entry *entry = RC_OBTAIN_OBJECT_ENTRY(map + i);
		int path = entry->path;
		struct object *obj;
		int index = i;

		i += RC_ACTUAL_OBJECT_ENTRY_SIZE(entry);

		/* add extra objects if necessary */
		if (entry->type != OBJ_COMMIT)
			continue;
		else
			consume_children = 0;

		if (path >= total_path_nr)
			goto end;

		/* in one of our branches?
		 * uninteresting trumps interesting */
		if (entry->include)
			paths[path] |= entry->uninteresting ? UPATH : IPATH;
		else if (!paths[path])
			continue;

		/* date stuff */
		if (revs->max_age != -1 && entry->date < revs->max_age)
			paths[path] |= UPATH;

		/* lookup object */
		co = lookup_commit(entry->sha1);
		obj = &co->object;

		if (obj->flags & UNINTERESTING)
			paths[path] |= UPATH;

		if ((paths[path] & IPATH) && (paths[path] & UPATH)) {
			paths[path] = UPATH;

			/* mark edge */
			if (last_objects[path]) {
				parse_commit(last_objects[path]);

				last_objects[path]->object.flags &= ~FACE_VALUE;
				last_objects[path] = 0;
			}
		}

		/* now we gotta re-assess the whole interesting thing... */
		entry->uninteresting = !!(paths[path] & UPATH);

		/* first close paths */
		if (entry->split_nr) {
			int j, off = index + sizeof(struct rc_object_entry_ondisk) + RC_PATH_SIZE(entry->merge_nr);

			for (j = 0; j < entry->split_nr; j++) {
				unsigned short p = ntohs(*(uint16_t *)(map + off + RC_PATH_SIZE(j)));

				if (p >= total_path_nr)
					goto end;

				/* boundary commit? */
				if ((paths[p] & IPATH) && entry->uninteresting) {
					if (last_objects[p]) {
						parse_commit(last_objects[p]);

						last_objects[p]->object.flags &= ~FACE_VALUE;
						last_objects[p] = 0;
					}
				} else if (last_objects[p] && !last_objects[p]->object.parsed)
					commit_list_insert(co, &last_objects[p]->parents);

				/* can't close a merge path until all are parents have been encountered */
				if (GET_COUNT(paths[p])) {
					SET_COUNT(paths[p], GET_COUNT(paths[p]) - 1);

					if (GET_COUNT(paths[p]))
						continue;
				}

				paths[p] = 0;
				last_objects[p] = 0;
			}
		}

		/* make topo relations */
		if (last_objects[path] && !last_objects[path]->object.parsed)
			commit_list_insert(co, &last_objects[path]->parents);

		/* initialize commit */
		if (!entry->is_end) {
			co->date = entry->date;
			obj->flags |= ADDED | FACE_VALUE;
		} else
			parse_commit(co);

		obj->flags |= SEEN;

		if (entry->uninteresting)
			obj->flags |= UNINTERESTING;

		/* we need to know what the edges are */
		last_objects[path] = co;

		/* add to list */
		if (!(obj->flags & UNINTERESTING) || revs->show_all) {
			if (entry->is_end)
				insert_by_date_cached(co, work, insert_cache, &insert_cache);
			else
				*queue = &commit_list_insert(co, *queue)->next;

			/* add children to list as well */
			if (obj->flags & UNINTERESTING)
				consume_children = 0;
			else
				consume_children = 1;
		}

		/* open parents */
		if (entry->merge_nr) {
			int j, off = index + sizeof(struct rc_object_entry_ondisk);
			char flag = entry->uninteresting ? UPATH : IPATH;

			for (j = 0; j < entry->merge_nr; j++) {
				unsigned short p = ntohs(*(uint16_t *)(map + off + RC_PATH_SIZE(j)));

				if (p >= total_path_nr)
					goto end;

				if (paths[p] & flag)
					continue;

				paths[p] |= flag;
			}

			/* make sure we don't use this path before all our parents have had their say */
			SET_COUNT(paths[path], entry->merge_nr);
		}

	}

	retval = 0;

end:
	free(paths);
	free(last_objects);

	return retval;
}

static int get_cache_slice_header(unsigned char *cache_sha1, unsigned char *map, int len, struct rc_slice_header *head)
{
	int t;

	memcpy(head, map, sizeof(struct rc_slice_header));
	head->ofs_objects = ntohl(head->ofs_objects);
	head->object_nr = ntohl(head->object_nr);
	head->size = ntohl(head->size);
	head->path_nr = ntohs(head->path_nr);

	if (memcmp(head->signature, "REVCACHE", 8))
		return -1;
	if (head->version != SUPPORTED_REVCACHE_VERSION)
		return -2;
	if (hashcmp(head->sha1, cache_sha1))
		return -3;
	t = sizeof(struct rc_slice_header);
	if (t != head->ofs_objects || t >= len)
		return -4;

	head->size = len;

	return 0;
}

int traverse_cache_slice(struct rev_info *revs,
	unsigned char *cache_sha1, struct commit *commit,
	unsigned long *date_so_far, int *slop_so_far,
	struct commit_list ***queue, struct commit_list **work)
{
	int fd = -1, retval = -3;
	struct stat fi;
	struct rc_slice_header head;
	struct rev_cache_info *rci;
	unsigned char *map = MAP_FAILED;

	/* the index should've been loaded already to find cache_sha1, but it's good
	 * to be absolutely sure... */
	if (!idx_map)
		init_index();
	if (!idx_map)
		return -1;

	/* load options */
	rci = &revs->rev_cache_info;

	memset(&head, 0, sizeof(struct rc_slice_header));

	fd = open(git_path("rev-cache/%s", sha1_to_hex(cache_sha1)), O_RDONLY);
	if (fd == -1)
		goto end;
	if (fstat(fd, &fi) || fi.st_size < sizeof(struct rc_slice_header))
		goto end;

	map = xmmap(0, fi.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED)
		goto end;
	if (get_cache_slice_header(cache_sha1, map, fi.st_size, &head))
		goto end;

	retval = traverse_cache_slice_1(&head, map, revs, commit, date_so_far, slop_so_far, queue, work);

end:
	if (map != MAP_FAILED)
		munmap(map, fi.st_size);
	if (fd != -1)
		close(fd);

	return retval;
}



/* generation */

struct path_track {
	struct commit *commit;
	int path; /* for keeping track of children */

	struct path_track *next, *prev;
};

static unsigned char *paths;
static int path_nr = 1, path_sz;

static struct path_track *path_track;
static struct path_track *path_track_alloc;

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

static void add_path_to_track(struct commit *commit, int path)
{
	make_path_track(&path_track, commit);
	path_track->path = path;
}

static void handle_paths(struct commit *commit, struct rc_object_entry *object, struct strbuf *merge_str, struct strbuf *split_str)
{
	int child_nr, parent_nr, open_parent_nr, this_path;
	struct commit_list *list;
	struct commit *first_parent;
	struct path_track **ppt, *pt;

	/* we can only re-use a closed path once all it's children have been encountered,
	 * as we need to keep track of commit boundaries */
	ppt = &path_track;
	pt = *ppt;
	child_nr = 0;
	while (pt) {
		if (pt->commit == commit) {
			uint16_t write_path;

			if (paths[pt->path] != PATH_IN_USE)
				paths[pt->path]--;

			/* make sure we can handle this */
			child_nr++;
			if (child_nr > 0x7f)
				die("%s: too many branches!  rev-cache can only handle %d parents/children per commit",
					sha1_to_hex(object->sha1), 0x7f);

			/* add to split list */
			object->split_nr++;
			write_path = htons((uint16_t)pt->path);
			strbuf_add(split_str, &write_path, sizeof(uint16_t));

			remove_path_track(ppt, 0);
			pt = *ppt;
		} else {
			pt = pt->next;
			ppt = &pt;
		}
	}

	/* initialize our self! */
	if (!commit->indegree) {
		commit->indegree = get_new_path();
		object->is_start = 1;
	}

	this_path = commit->indegree;
	paths[this_path] = PATH_IN_USE;
	object->path = this_path;

	/* count interesting parents */
	parent_nr = open_parent_nr = 0;
	first_parent = 0;
	for (list = commit->parents; list; list = list->next) {
		if (list->item->object.flags & UNINTERESTING) {
			object->is_end = 1;
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

	if (parent_nr == 1 && open_parent_nr == 1) {
		first_parent->indegree = this_path;
		return;
	}

	/* bail out on obscene parent/child #s */
	if (parent_nr > 0x7f)
		die("%s: too many parents in merge!  rev-cache can only handle %d parents/children per commit",
			sha1_to_hex(object->sha1), 0x7f);

	/* make merge list */
	object->merge_nr = parent_nr;
	paths[this_path] = parent_nr;

	for (list = commit->parents; list; list = list->next) {
		struct commit *p = list->item;
		uint16_t write_path;

		if (p->object.flags & UNINTERESTING)
			continue;

		/* unfortunately due to boundary tracking we can't re-use merge paths
		 * (unable to guarantee last parent path = this -> last won't always be able to
		 * set this as a boundary object */
		if (!p->indegree)
			p->indegree = get_new_path();

		write_path = htons((uint16_t)p->indegree);
		strbuf_add(merge_str, &write_path, sizeof(uint16_t));

		/* make sure path is properly ended */
		add_path_to_track(p, this_path);
	}

}


static void add_object_entry(const unsigned char *sha1, int type, struct rc_object_entry *nothisone,
	struct strbuf *merge_str, struct strbuf *split_str)
{
	struct rc_object_entry object;

	if (!nothisone) {
		memset(&object, 0, sizeof(object));
		object.sha1 = (unsigned char *)sha1;
		object.type = type;

		if (merge_str)
			object.merge_nr = merge_str->len / sizeof(uint16_t);
		if (split_str)
			object.split_nr = split_str->len / sizeof(uint16_t);

		nothisone = &object;
	}

	strbuf_add(acc_buffer, to_disked_rc_object_entry(nothisone, 0), sizeof(struct rc_object_entry_ondisk));

	if (merge_str && merge_str->len)
		strbuf_add(acc_buffer, merge_str->buf, merge_str->len);
	if (split_str && split_str->len)
		strbuf_add(acc_buffer, split_str->buf, split_str->len);

}

static void init_revcache_directory(void)
{
	struct stat fi;

	if (stat(git_path("rev-cache"), &fi) || !S_ISDIR(fi.st_mode))
		if (mkdir(git_path("rev-cache"), 0777))
			die("can't make rev-cache directory");

}

void init_rev_cache_info(struct rev_cache_info *rci)
{
	rci->objects = 1;
	rci->legs = 0;
	rci->make_index = 1;

	rci->add_to_pending = 1;

	rci->ignore_size = 0;
}

void maybe_fill_with_defaults(struct rev_cache_info *rci)
{
	static struct rev_cache_info def_rci;

	if (rci)
		return;

	init_rev_cache_info(&def_rci);
	rci = &def_rci;
}

int make_cache_slice(struct rev_cache_info *rci,
	struct rev_info *revs, struct commit_list **starts, struct commit_list **ends,
	unsigned char *cache_sha1)
{
	struct rev_info therevs;
	struct strbuf buffer, startlist, endlist;
	struct rc_slice_header head;
	struct commit *commit;
	unsigned char sha1[20];
	struct strbuf merge_paths, split_paths;
	int object_nr, total_sz, fd;
	char file[PATH_MAX], *newfile;
	struct rev_cache_info *trci;
	git_SHA_CTX ctx;

	maybe_fill_with_defaults(rci);

	init_revcache_directory();
	strcpy(file, git_path("rev-cache/XXXXXX"));
	fd = xmkstemp(file);

	strbuf_init(&buffer, 0);
	strbuf_init(&startlist, 0);
	strbuf_init(&endlist, 0);
	strbuf_init(&merge_paths, 0);
	strbuf_init(&split_paths, 0);
	acc_buffer = &buffer;

	if (!revs) {
		revs = &therevs;
		init_revisions(revs, 0);

		/* we're gonna assume no one else has already traversed this... */
		while ((commit = pop_commit(starts)))
			add_pending_object(revs, &commit->object, 0);

		while ((commit = pop_commit(ends))) {
			commit->object.flags |= UNINTERESTING;
			add_pending_object(revs, &commit->object, 0);
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

	/* re-use info from other caches if possible */
	trci = &revs->rev_cache_info;
	init_rev_cache_info(trci);
	trci->add_to_pending = 0;

	setup_revisions(0, 0, revs, 0);
	if (prepare_revision_walk(revs))
		die("died preparing revision walk");

	object_nr = total_sz = 0;
	while ((commit = get_revision(revs)) != 0) {
		struct rc_object_entry object;

		strbuf_setlen(&merge_paths, 0);
		strbuf_setlen(&split_paths, 0);

		memset(&object, 0, sizeof(object));
		object.type = OBJ_COMMIT;
		object.date = commit->date;
		object.sha1 = commit->object.sha1;

		handle_paths(commit, &object, &merge_paths, &split_paths);

		if (object.is_end) {
			strbuf_add(&endlist, object.sha1, 20);
			if (ends)
				commit_list_insert(commit, ends);
		}
		/* the two *aren't* mutually exclusive */
		if (object.is_start) {
			strbuf_add(&startlist, object.sha1, 20);
			if (starts)
				commit_list_insert(commit, starts);
		}

		commit->indegree = 0;

		add_object_entry(0, 0, &object, &merge_paths, &split_paths);
		object_nr++;

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

	/* go ahead a free some stuff... */
	strbuf_release(&buffer);
	strbuf_release(&merge_paths);
	strbuf_release(&split_paths);
	if (path_sz)
		free(paths);
	while (path_track_alloc)
		remove_path_track(&path_track_alloc, 1);

	/* the meaning of the hash name is more or less irrelevant, it's the uniqueness that matters */
	strbuf_add(&endlist, startlist.buf, startlist.len);
	git_SHA1_Init(&ctx);
	git_SHA1_Update(&ctx, endlist.buf, endlist.len);
	git_SHA1_Final(sha1, &ctx);

	/* now actually initialize header */
	strcpy(head.signature, "REVCACHE");
	head.version = SUPPORTED_REVCACHE_VERSION;

	head.object_nr = htonl(object_nr);
	head.size = htonl(ntohl(head.ofs_objects) + total_sz);
	head.path_nr = htons(path_nr);
	hashcpy(head.sha1, sha1);

	/* some info! */
	fprintf(stderr, "objects: %d\n", object_nr);
	fprintf(stderr, "paths: %d\n", path_nr);

	lseek(fd, 0, SEEK_SET);
	xwrite(fd, &head, sizeof(head));

	if (rci->make_index && make_cache_index(rci, sha1, fd, ntohl(head.size)) < 0)
		die("can't update index");

	close(fd);

	newfile = git_path("rev-cache/%s", sha1_to_hex(sha1));
	if (rename(file, newfile))
		die("can't move temp file");

	/* let our caller know what we've just made */
	if (cache_sha1)
		hashcpy(cache_sha1, sha1);

	strbuf_release(&endlist);
	strbuf_release(&startlist);

	return 0;
}


static int index_sort_hash(const void *a, const void *b)
{
	return hashcmp(((struct rc_index_entry_ondisk *)a)->sha1, ((struct rc_index_entry_ondisk *)b)->sha1);
}

static int write_cache_index(struct strbuf *body)
{
	struct rc_index_header whead;
	struct lock_file *lk;
	int fd, i;

	/* clear index map if loaded */
	if (idx_map) {
		munmap(idx_map, idx_size);
		idx_map = 0;
	}

	lk = xcalloc(sizeof(struct lock_file), 1);
	fd = hold_lock_file_for_update(lk, git_path("rev-cache/index"), 0);
	if (fd < 0) {
		free(lk);
		return -1;
	}

	/* endianness yay! */
	memset(&whead, 0, sizeof(whead));
	memcpy(whead.signature, "REVINDEX", 8);
	whead.version = idx_head.version;
	whead.ofs_objects = htonl(idx_head.ofs_objects);
	whead.object_nr = htonl(idx_head.object_nr);
	whead.cache_nr = idx_head.cache_nr;
	whead.max_date = htonl(idx_head.max_date);

	write(fd, &whead, sizeof(struct rc_index_header));
	write_in_full(fd, idx_caches, idx_head.cache_nr * 20);

	for (i = 0; i <= 0xff; i++)
		fanout[i] = htonl(fanout[i]);
	write_in_full(fd, fanout, 0x100 * sizeof(uint32_t));

	write_in_full(fd, body->buf, body->len);

	if (commit_lock_file(lk) < 0)
		return -2;

	/* lk freed by lockfile.c */

	return 0;
}

int make_cache_index(struct rev_cache_info *rci, unsigned char *cache_sha1,
	int fd, unsigned int size)
{
	struct strbuf buffer;
	int i, cache_index, cur;
	unsigned char *map;
	unsigned long max_date;

	if (!idx_map)
		init_index();

	lseek(fd, 0, SEEK_SET);
	map = xmmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED)
		return -1;

	strbuf_init(&buffer, 0);
	if (idx_map) {
		strbuf_add(&buffer, idx_map + fanout[0], fanout[0x100] - fanout[0]);
	} else {
		/* not an update */
		memset(&idx_head, 0, sizeof(struct rc_index_header));
		idx_caches = 0;

		strcpy(idx_head.signature, "REVINDEX");
		idx_head.version = SUPPORTED_REVINDEX_VERSION;
		idx_head.ofs_objects = sizeof(struct rc_index_header) + 0x100 * sizeof(uint32_t);
	}

	/* are we remaking a slice? */
	for (i = 0; i < idx_head.cache_nr; i++)
		if (!hashcmp(idx_caches + i * 20, cache_sha1))
			break;

	if (i == idx_head.cache_nr) {
		cache_index = idx_head.cache_nr++;
		idx_head.ofs_objects += 20;

		idx_caches = xrealloc(idx_caches, idx_head.cache_nr * 20);
		hashcpy(idx_caches + cache_index * 20, cache_sha1);
	} else
		cache_index = i;

	i = sizeof(struct rc_slice_header); /* offset */
	max_date = idx_head.max_date;
	while (i < size) {
		struct rc_index_entry index_entry, *entry;
		struct rc_index_entry_ondisk *disked_entry;
		struct rc_object_entry *object_entry = RC_OBTAIN_OBJECT_ENTRY(map + i);
		unsigned long date;
		int off, pos = i;

		i += RC_ACTUAL_OBJECT_ENTRY_SIZE(object_entry);

		if (object_entry->type != OBJ_COMMIT)
			continue;

		/* don't include ends; otherwise we'll find ourselves in loops */
		if (object_entry->is_end)
			continue;

		/* handle index duplication
		 * -> keep old copy unless new one is a start -- based on expected usage, older ones will be more
		 * likely to lead to greater slice traversals than new ones */
		date = object_entry->date;
		if (date > idx_head.max_date) {
			disked_entry = 0;
			if (date > max_date)
				max_date = date;
		} else
			disked_entry = search_index_1(object_entry->sha1);

		if (disked_entry && !object_entry->is_start)
			continue;
		else if (disked_entry) {
			/* mmm, pointer arithmetic... tasty */  /* (entry - idx_map = offset, so cast is valid) */
			off = (unsigned int)((unsigned char *)disked_entry - idx_map) - fanout[0];
			disked_entry = (struct rc_index_entry_ondisk *)(buffer.buf + off);
			entry = from_disked_rc_index_entry(disked_entry, 0);
		} else
			entry = &index_entry;

		memset(entry, 0, sizeof(index_entry));
		entry->sha1 = object_entry->sha1;
		entry->is_start = object_entry->is_start;
		entry->cache_index = cache_index;
		entry->pos = pos;

		if (entry == &index_entry) {
			strbuf_add(&buffer, to_disked_rc_index_entry(entry, 0), sizeof(struct rc_index_entry_ondisk));
			idx_head.object_nr++;
		} else
			to_disked_rc_index_entry(entry, disked_entry);

	}

	idx_head.max_date = max_date;
	qsort(buffer.buf, buffer.len / sizeof(struct rc_index_entry_ondisk), sizeof(struct rc_index_entry_ondisk), index_sort_hash);

	/* generate fanout */
	cur = 0x00;
	for (i = 0; i < buffer.len; i += sizeof(struct rc_index_entry_ondisk)) {
		struct rc_index_entry_ondisk *entry = (struct rc_index_entry_ondisk *)(buffer.buf + i);

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

	/* idx_map is unloaded without cleanup_cache_slices(), so regardless of previous index existence
	 * we can still free this up */
	free(idx_caches);

	return 0;
}


/* add start-commits from each cache slice (uninterestingness will be propogated) */
void starts_from_slices(struct rev_info *revs, unsigned int flags)
{
	struct commit *commit;
	int i;

	if (!idx_map)
		init_index();
	if (!idx_map)
		return;

	for (i = idx_head.ofs_objects; i < idx_size; i += sizeof(struct rc_index_entry_ondisk)) {
		struct rc_index_entry *entry = RC_OBTAIN_INDEX_ENTRY(idx_map + i);

		if (!entry->is_start)
			continue;

		commit = lookup_commit(entry->sha1);
		if (!commit)
			continue;

		commit->object.flags |= flags;
		add_pending_object(revs, &commit->object, 0);
	}

}
