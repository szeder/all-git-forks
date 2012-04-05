#include "cache.h"
#include "tag.h"
#include "commit.h"
#include "tree.h"
#include "blob.h"
#include "diff.h"
#include "tree-walk.h"
#include "revision.h"
#include "list-objects.h"

static void process_blob(struct rev_info *revs,
			 struct blob *blob,
			 show_object_fn show,
			 struct name_path *path,
			 const char *name,
			 void *cb_data)
{
	struct object *obj = &blob->object;

	if (!revs->blob_objects)
		return;
	if (!obj)
		die("bad blob object");
	if (obj->flags & (UNINTERESTING | SEEN))
		return;
	obj->flags |= SEEN;
	show(obj, path, name, cb_data);
}

/*
 * Processing a gitlink entry currently does nothing, since
 * we do not recurse into the subproject.
 *
 * We *could* eventually add a flag that actually does that,
 * which would involve:
 *  - is the subproject actually checked out?
 *  - if so, see if the subproject has already been added
 *    to the alternates list, and add it if not.
 *  - process the commit (or tag) the gitlink points to
 *    recursively.
 *
 * However, it's unclear whether there is really ever any
 * reason to see superprojects and subprojects as such a
 * "unified" object pool (potentially resulting in a totally
 * humongous pack - avoiding which was the whole point of
 * having gitlinks in the first place!).
 *
 * So for now, there is just a note that we *could* follow
 * the link, and how to do it. Whether it necessarily makes
 * any sense what-so-ever to ever do that is another issue.
 */
static void process_gitlink(struct rev_info *revs,
			    const unsigned char *sha1,
			    show_object_fn show,
			    struct name_path *path,
			    const char *name,
			    void *cb_data)
{
	/* Nothing to do */
}

static void process_tree(struct rev_info *revs,
			 struct tree *tree,
			 show_object_fn show,
			 struct name_path *path,
			 struct strbuf *base,
			 const char *name,
			 void *cb_data);
static int process_one_tree(int *match, struct name_entry *entry,
			    struct strbuf *base, struct rev_info *revs,
			    show_object_fn show, struct name_path *me,
			    void *cb_data)
{
	if (*match != all_entries_interesting) {
		*match = tree_entry_interesting(entry, base, 0,
					       &revs->diffopt.pathspec);
		if (*match == all_entries_not_interesting)
			return -1;
		if (*match == entry_not_interesting)
			return 0;
	}

	if (S_ISDIR(entry->mode))
		process_tree(revs,
			     lookup_tree(entry->sha1),
			     show, me, base, entry->path,
			     cb_data);
	else if (S_ISGITLINK(entry->mode))
		process_gitlink(revs, entry->sha1,
				show, me, entry->path,
				cb_data);
	else
		process_blob(revs,
			     lookup_blob(entry->sha1),
			     show, me, entry->path,
			     cb_data);
	return 0;
}

struct idx_entry {
	unsigned char sha1[20];
	uint32_t ptr;
};

static uintmax_t decode_varint(const unsigned char **bufp)
{
	const unsigned char *buf = *bufp;
	unsigned char c = *buf++;
	uintmax_t val = c & 127;
	while (c & 128) {
		val += 1;
		if (!val || MSB(val, 7))
			return 0; /* overflow */
		c = *buf++;
		val = (val << 7) + (c & 127);
	}
	*bufp = buf;
	return val;
}

static int tree_cache;
static const unsigned char *idx = NULL;
static const unsigned char *cache = NULL;
static uint32_t path_start, entry_start;
static const uint32_t *array;
static const struct idx_entry *idx_entries;
static unsigned long nr_entries, nr_unique;

static void open_tree_cache()
{
	struct strbuf sb = STRBUF_INIT;
	struct stat st;
	int fd;

	strbuf_addstr(&sb, getenv("TREE_CACHE"));
	if (stat(sb.buf, &st))
		return;
	fd = open(sb.buf, O_RDONLY);
	if (fd == -1)
		return;
#if 1
	cache = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (!cache)
		return;
#else
	cache = xmalloc(st.st_size);
	if (!cache)
		return;
	xread(fd, cache, st.st_size);
#endif
	close(fd);

	strbuf_addstr(&sb, ".idx");
	if (stat(sb.buf, &st))
		return;
	fd = open(sb.buf, O_RDONLY);
	if (fd == -1)
		return;
	idx = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (!idx)
		return;
	close(fd);

	entry_start = ((uint32_t*)idx)[0];
	nr_unique = ((uint32_t*)idx)[1];
	path_start = ((uint32_t*)idx)[2];
	array = (uint32_t*)(idx + 12);
	idx_entries = (struct idx_entry *)(array + 256);
	nr_entries = array[255];

	fprintf(stderr, "tree cache opened, %lu trees, %lu leaves\n",
		nr_entries, nr_unique);
	strbuf_release(&sb);
}

static void process_tree(struct rev_info *revs,
			 struct tree *tree,
			 show_object_fn show,
			 struct name_path *path,
			 struct strbuf *base,
			 const char *name,
			 void *cb_data)
{
	struct object *obj = &tree->object;
	struct tree_desc desc;
	struct name_entry entry;
	struct name_path me;
	enum interesting match = revs->diffopt.pathspec.nr == 0 ?
		all_entries_interesting: entry_not_interesting;
	int baselen = base->len;

	if (!revs->tree_objects)
		return;
	if (!obj)
		die("bad tree object");
	if (obj->flags & (UNINTERESTING | SEEN))
		return;
	if (!tree_cache && parse_tree(tree) < 0)
		die("bad tree object %s", sha1_to_hex(obj->sha1));
	obj->flags |= SEEN;
	show(obj, path, name, cb_data);
	me.up = path;
	me.elem = name;
	me.elem_len = strlen(name);

	if (!match) {
		strbuf_addstr(base, name);
		if (base->len)
			strbuf_addch(base, '/');
	}

	while (tree_cache) {
		const struct idx_entry *result;
		unsigned char fanout;
		unsigned long lo, hi;
		const unsigned char *start;
		unsigned long i;

		if (!idx && !cache)
			open_tree_cache();

		if (!idx || !cache)
			break;

		fanout = tree->object.sha1[0];
		lo = fanout ? array[fanout-1] : 0;
		hi = array[fanout];
		result = NULL;

		do {
			unsigned mi = (lo + hi) / 2;
			int cmp = hashcmp((idx_entries + mi)->sha1, tree->object.sha1);

			if (!cmp) {
				result = idx_entries + mi;
				break;
			}
			if (cmp > 0)
				hi = mi;
			else
				lo = mi+1;
		} while (lo < hi);

		if (!result)
			break;

		start = cache + result->ptr;
		while ((i = decode_varint(&start)) != 0) {
			const uint32_t *mode = (uint32_t*)(cache + entry_start);
			const unsigned char *sha1 = (unsigned char*)mode + sizeof(uint32_t) * nr_unique;
			const uint32_t *path_idx = (uint32_t*)(sha1 + 20 * nr_unique);

			entry.mode = mode[i];
			entry.sha1 = sha1 + i * 20;
			entry.path = (const char*)cache + path_start + path_idx[i];
			if (process_one_tree(&match, &entry, base, revs,
					     show, &me, cb_data))
				break;
		}

		strbuf_setlen(base, baselen);
		return;
	}

	init_tree_desc(&desc, tree->buffer, tree->size);

	while (tree_entry(&desc, &entry)) {
		if (process_one_tree(&match, &entry, base, revs,
				     show, &me, cb_data))
			break;
	}
	strbuf_setlen(base, baselen);
	free(tree->buffer);
	tree->buffer = NULL;
}

static void mark_edge_parents_uninteresting(struct commit *commit,
					    struct rev_info *revs,
					    show_edge_fn show_edge)
{
	struct commit_list *parents;

	for (parents = commit->parents; parents; parents = parents->next) {
		struct commit *parent = parents->item;
		if (!(parent->object.flags & UNINTERESTING))
			continue;
		mark_tree_uninteresting(parent->tree);
		if (revs->edge_hint && !(parent->object.flags & SHOWN)) {
			parent->object.flags |= SHOWN;
			show_edge(parent);
		}
	}
}

void mark_edges_uninteresting(struct commit_list *list,
			      struct rev_info *revs,
			      show_edge_fn show_edge)
{
	for ( ; list; list = list->next) {
		struct commit *commit = list->item;

		if (commit->object.flags & UNINTERESTING) {
			mark_tree_uninteresting(commit->tree);
			continue;
		}
		mark_edge_parents_uninteresting(commit, revs, show_edge);
	}
}

static void add_pending_tree(struct rev_info *revs, struct tree *tree)
{
	add_pending_object(revs, &tree->object, "");
}

void traverse_commit_list(struct rev_info *revs,
			  show_commit_fn show_commit,
			  show_object_fn show_object,
			  void *data)
{
	int i;
	struct commit *commit;
	struct strbuf base;

	tree_cache = getenv("TREE_CACHE") != NULL;
	strbuf_init(&base, PATH_MAX);
	while ((commit = get_revision(revs)) != NULL) {
		/*
		 * an uninteresting boundary commit may not have its tree
		 * parsed yet, but we are not going to show them anyway
		 */
		if (commit->tree)
			add_pending_tree(revs, commit->tree);
		show_commit(commit, data);
	}
	for (i = 0; i < revs->pending.nr; i++) {
		struct object_array_entry *pending = revs->pending.objects + i;
		struct object *obj = pending->item;
		const char *name = pending->name;
		if (obj->flags & (UNINTERESTING | SEEN))
			continue;
		if (obj->type == OBJ_TAG) {
			obj->flags |= SEEN;
			show_object(obj, NULL, name, data);
			continue;
		}
		if (obj->type == OBJ_TREE) {
			process_tree(revs, (struct tree *)obj, show_object,
				     NULL, &base, name, data);
			continue;
		}
		if (obj->type == OBJ_BLOB) {
			process_blob(revs, (struct blob *)obj, show_object,
				     NULL, name, data);
			continue;
		}
		die("unknown pending object %s (%s)",
		    sha1_to_hex(obj->sha1), name);
	}
	if (revs->pending.nr) {
		free(revs->pending.objects);
		revs->pending.nr = 0;
		revs->pending.alloc = 0;
		revs->pending.objects = NULL;
	}
	strbuf_release(&base);
}
