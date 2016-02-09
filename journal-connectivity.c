#include "cache.h"
#include "commit.h"
#include "hashmap.h"
#include "lmdb-utils.h"
#include "lockfile.h"
#include "journal-connectivity.h"
#include "pack.h"
#include "pathspec.h"
#include "refs.h"
#include "sha1-array.h"
#include "tag.h"
#include "tree.h"
#include "tree-walk.h"

static const char dbname[] = "connectivity-lmdb";

struct sha1_array_entry {
	struct hashmap_entry entry;
	unsigned char sha1[20];
	uint64_t *objectset;
	struct sha1_array children;
	unsigned processed : 1;
};

struct sha1_entry {
	struct hashmap_entry entry;
	unsigned char sha1[20];
	int id;
};

struct hashmap commits_by_sha;
struct hashmap all_objects;

static int sha1_array_entry_cmp(const struct sha1_array_entry *e1,
				const struct sha1_array_entry *e2,
				const unsigned char *keydata)
{
	return hashcmp(e1->sha1, e2->sha1);
}

static int sha1_entry_cmp(const struct sha1_entry *e1,
			  const struct sha1_entry *e2,
			  const unsigned char *keydata)
{
	return hashcmp(e1->sha1, e2->sha1);
}

static int object_nr = 0;
static int objectset_index(const unsigned char *sha1)
{
	struct sha1_entry entry;
	struct sha1_entry *existing;

	hashcpy(entry.sha1, sha1);
	hashmap_entry_init(&entry, sha1hash(sha1));
	existing = hashmap_get(&all_objects, &entry, NULL);
	assert(existing);

	return existing->id;
}

static int objectset_contains_index(const uint64_t *objectset, int index)
{
	uint64_t chunk = objectset[index / 64];
	return chunk & (1ULL << (index % 64)) ? 1 : 0;
}

static void objectset_add_index(uint64_t *objectset, int index)
{
	uint64_t *chunk = &objectset[index / 64];
	*chunk |= (1ULL << (index % 64));
}

struct connectivity_ctx {
	uint64_t **parent_objectsets;
	uint64_t *my_objects;
	struct sha1_array *originated;
	int parent_nr;
};

static int add_new_object(const unsigned char *sha1, struct strbuf *base,
			  const char *pathname, unsigned mode, int stage,
			  void *context)
{
	struct connectivity_ctx *ctx = context;
	int i;
	int idx;

	if (S_ISGITLINK(mode))
		return 0;

	idx = objectset_index(sha1);

	for (i = 0; i < ctx->parent_nr; i++) {
		if (objectset_contains_index(ctx->parent_objectsets[i], idx))
			return 0;
	}

	sha1_array_append(ctx->originated, sha1);
	objectset_add_index(ctx->my_objects, idx);
	return READ_TREE_RECURSIVE;
}

static void objectset_update(uint64_t *dst, uint64_t *src)
{
	int i;
	for (i = 0; i < 1 + object_nr / 64; i++) {
		dst[i] |= src[i];
	}
}

static void get_originated_objects(struct commit *commit,
				   uint64_t **parent_objectsets,
				   uint64_t *my_objects,
				   struct sha1_array *originated,
				   int parent_nr)
{
	struct tree *tree;
	struct pathspec pathspec;
	struct connectivity_ctx ctx;
	int i;
	int idx;

	parse_commit_or_die(commit);

	tree = commit->tree;
	idx = objectset_index(tree->object.oid.hash);

	for (i = 0; i < parent_nr; i++) {
		if (objectset_contains_index(parent_objectsets[i],
					     idx))
			return;
	}

	memset(&pathspec, 0, sizeof(pathspec));
	pathspec.nr = 0;
	pathspec.recursive = 0;
	pathspec.magic = PATHSPEC_FROMTOP;

	ctx.parent_objectsets = parent_objectsets;
	ctx.my_objects = my_objects;
	ctx.originated = originated;
	ctx.parent_nr = parent_nr;

	read_tree_recursive(tree, "", 0, 0, &pathspec, add_new_object,
			    &ctx);

	objectset_add_index(my_objects, idx);
	sha1_array_append(originated, tree->object.oid.hash);
}

static int add_ref_to_roots(const char *refname, const struct object_id *oid,
			    int flags, void *cb_data)
{
	struct sha1_array *roots = cb_data;

	sha1_array_append(roots, oid->hash);

	return 0;
}

/* Returns 0 if the object has been inserted, 1 if it already existed */
static int all_objects_insert(const unsigned char *sha1)
{
	struct sha1_entry *entry = xmalloc(sizeof(*entry));
	struct sha1_entry *existing;

	entry->id = object_nr;
	hashcpy(entry->sha1, sha1);
	hashmap_entry_init(entry, sha1hash(sha1));
	existing = hashmap_put(&all_objects, entry);
	if (existing) {
		entry->id = existing->id;
		free(existing);
		return 1;
	}

	object_nr++;
	return 0;
}

static int initial_add_object(const unsigned char *sha1, struct strbuf *base,
			       const char *pathname, unsigned mode, int stage,
			       void *context)
{

	if (S_ISGITLINK(mode))
		return 0;
	if (!all_objects_insert(sha1))
		return READ_TREE_RECURSIVE;
	return 0;
}

struct pending_commit {
	unsigned char sha1[20];
	unsigned char child_sha1[20];
};

static struct pending_commit *commit_stack;
static size_t commit_stack_nr;
static size_t commit_stack_alloc;

static void commit_stack_push(const unsigned char *sha1, const unsigned char *child)
{
	ALLOC_GROW(commit_stack, commit_stack_nr + 1, commit_stack_alloc);

	hashcpy(commit_stack[commit_stack_nr].sha1, sha1);
	if (child)
		hashcpy(commit_stack[commit_stack_nr].child_sha1, child);
	else
		hashcpy(commit_stack[commit_stack_nr].child_sha1, null_sha1);

	commit_stack_nr ++;
}

static void commit_stack_pop(void)
{
	assert(commit_stack_nr);
	--commit_stack_nr;
}

static const struct pending_commit *commit_stack_top(void)
{
	return &commit_stack[commit_stack_nr - 1];
}

static void commit_stack_clear(void)
{
	commit_stack_nr = commit_stack_alloc = 0;
	free(commit_stack);
	commit_stack = NULL;
}

static void first_pass_object(struct object *obj);

static void first_pass_tag(struct tag *tag)
{
	if (!parse_tag(tag))
		first_pass_object(tag->tagged);
}

static void first_pass_commit(struct commit *commit, const unsigned char *child_sha)
{
	struct commit_list *cur;
	struct sha1_array_entry *e = xcalloc(1, sizeof(*e));
	struct sha1_array_entry *found;

	hashcpy(e->sha1, commit->object.oid.hash);
	hashmap_entry_init(e, sha1hash(e->sha1));
	found = hashmap_get(&commits_by_sha, e, NULL);
	if (found) {
		/*
		 * We've seen this commit before, but it has a new child.
		 */
		free(e);
		e = found;
	} else {
		struct pathspec pathspec;

		hashmap_add(&commits_by_sha, e);
		parse_commit_or_die(commit);

		for (cur = commit->parents; cur; cur = cur->next) {
			commit_stack_push(cur->item->object.oid.hash,
					  commit->object.oid.hash);
		}
		all_objects_insert(commit->tree->object.oid.hash);
		all_objects_insert(commit->object.oid.hash);

		memset(&pathspec, 0, sizeof(pathspec));
		pathspec.magic = PATHSPEC_FROMTOP;

		read_tree_recursive(commit->tree, "", 0, 0, &pathspec,
				    initial_add_object, NULL);

		free_commit_buffer(commit);
	}

	if (child_sha && !is_null_sha1(child_sha))
		sha1_array_append(&e->children, child_sha);
}

static void first_pass_object(struct object *obj)
{
	if (obj->type == OBJ_TAG)
		first_pass_tag((struct tag *)obj);

	else if (obj->type == OBJ_COMMIT)
		first_pass_commit((struct commit *)obj, NULL);

}

static void first_pass_sha(const unsigned char sha1[20], void *data)
{
	struct object *obj;

	obj = parse_object(sha1);
	if (!obj)
		return;
	first_pass_object(obj);
}

enum record_type {
	REC_PACK = 1,

	REC_COMMIT = 1000,
	REC_OBJ = 1001,
};

static uint32_t get_ref_count(const unsigned char *sha1)
{
	MDB_val key, val;
	uint32_t *fields;

	key.mv_data = (void *)sha1;
	key.mv_size = 20;
	if (mdb_get_or_die(&key, &val))
		return 0;

	fields = val.mv_data;
	return ntohl(fields[1]);
}

static void compute_originated_objects(struct commit *commit);

static void incref(const unsigned char *sha1, int full)
{
	MDB_cursor *cursor;
	MDB_val key, val, new_val;
	int ret;
	uint32_t count = 0;
	uint32_t *fields;

	mdb_cursor_open_or_die(&cursor);

	key.mv_data = (void *)sha1;
	key.mv_size = 20;
	ret = mdb_cursor_get_or_die(cursor, &key, &val, MDB_SET_KEY);
	if (ret) {
		if (full) {
			struct object *obj = parse_object(sha1);
			if (obj->type == OBJ_COMMIT) {
				struct commit *commit = (struct commit *)obj;
				compute_originated_objects(commit);
			} else if (obj->type == OBJ_TAG) {
				struct tag *tag = (struct tag *)obj;
				incref(tag->tagged->oid.hash, 0);
			}
		}

		fields = xmalloc(8);
		fields[0] = htonl(REC_OBJ);
		new_val.mv_size = 8;
	} else {
		fields = xmalloc(val.mv_size);
		memcpy(fields, val.mv_data, val.mv_size);
		count = ntohl(fields[1]);
		new_val.mv_size = val.mv_size;
	}

	fields[1] = htonl(count + 1);
	new_val.mv_data = fields;
	if (ret)
		mdb_put_or_die(&key, &new_val, 0);
	else
		assert(!mdb_cursor_put(cursor, &key, &new_val, MDB_CURRENT));
	mdb_cursor_close(cursor);
	free(fields);
}

static void append_sha1(const unsigned char sha1[20], void *data)
{
	struct strbuf *buf = data;
	strbuf_add(buf, sha1, 20);
}

static void incref_sha1(const unsigned char sha1[20], void *data)
{
	incref(sha1, 0);
}

static void store_originated_objects(const unsigned char *new_sha1,
				     struct sha1_array *objects)
{
	struct strbuf buf = STRBUF_INIT;
	uint32_t record_type = htonl(REC_COMMIT);
	uint32_t ref_count = get_ref_count(new_sha1);
	int object_count;
	MDB_val key, val;

	strbuf_add(&buf, &record_type, 4);

	ref_count = htonl(ref_count);
	strbuf_add(&buf, &ref_count, 4);

	object_count = htonl(sha1_array_count_unique(objects));
	strbuf_add(&buf, &object_count, 4);

	sha1_array_for_each_unique(objects, append_sha1, &buf);
	sha1_array_for_each_unique(objects, incref_sha1, NULL);

	key.mv_data = (void *)new_sha1;
	key.mv_size = 20;

	val.mv_data = buf.buf;
	val.mv_size = buf.len;

	mdb_put_or_die(&key, &val, 0);

	strbuf_release(&buf);
}

int jcdb_transaction_begin(struct jcdb_transaction *transaction,
			   enum jcdb_transaction_flags flags)
{
	transaction->open = 0;
	if (lmdb_init(dbname, 0, flags & JCDB_CREATE))
		return -1;

	transaction->open = 1;
	return 0;
}

/*
 * Record a pack in the connectivity data.  Return 1 if it was already present.
 */
int jcdb_add_pack(struct jcdb_transaction *transaction,
				const unsigned char *sha1)
{
	uint32_t record_type = htonl(REC_PACK);
	MDB_val key, val;
	int new;

	assert(transaction->open);

	key.mv_data = (void *)sha1;
	key.mv_size = 20;

	new = mdb_get_or_die(&key, &val);
	if (!new)
		return 1;

	val.mv_data = &record_type;
	val.mv_size = 4;

	mdb_put_or_die(&key, &val, 0);

	return 0;
}

void jcdb_transaction_commit(struct jcdb_transaction *transaction)
{
	assert(transaction->open);
	lmdb_txn_commit();
	transaction->open = 0;
}

void jcdb_transaction_abort(struct jcdb_transaction *transaction)
{
	assert(transaction->open);
	lmdb_txn_abort();
	transaction->open = 0;
}

void jcdb_packlog_dump(void)
{
	MDB_cursor *cursor;
	MDB_val key, val;
	int ret;

	/* No db? No packs. */
	if (lmdb_init(dbname, MDB_RDONLY, 0))
		return;

	mdb_cursor_open_or_die(&cursor);

	/*
	 * This is a bit slow because it has to iterate over every
	 * value in the db, but it's only for debugging so that's okay.
	 */
	key.mv_data = "aabbccddeeffgghhiijj";
	ret = mdb_cursor_get_or_die(cursor, &key, &val, MDB_FIRST);
	while (!ret) {
		uint32_t *fields = val.mv_data;

		if (ntohl(fields[0]) == REC_PACK)
			printf("%s\n", sha1_to_hex(key.mv_data));
		ret = mdb_cursor_get_or_die(cursor, &key, &val, MDB_NEXT);
	}
}

static int add_if_new(const unsigned char *sha1, struct strbuf *base,
		      const char *pathname, unsigned mode, int stage,
		      void *data)
{
	struct sha1_array *originated = data;

	if (get_ref_count(sha1))
		return 0;
	sha1_array_append(originated, sha1);
	return READ_TREE_RECURSIVE;
}

static void compute_originated_objects(struct commit *commit)
{
	const unsigned char *sha1 = commit->object.oid.hash;
	struct tree *tree = commit->tree;
	struct sha1_array originated = SHA1_ARRAY_INIT;
	struct commit_list *parent;
	struct pathspec pathspec;

	memset(&pathspec, 0, sizeof(pathspec));
	pathspec.nr = 0;
	pathspec.recursive = 0;
	pathspec.magic = PATHSPEC_FROMTOP;

	for (parent = commit->parents; parent; parent = parent->next)
		sha1_array_append(&originated, parent->item->object.oid.hash);

	read_tree_recursive(tree, "", 0, 0, &pathspec,
			    add_if_new, &originated);
	store_originated_objects(sha1, &originated);
	sha1_array_clear(&originated);
}

static void recursive_decref(const unsigned char *sha1)
{
	MDB_cursor *cursor;
	MDB_val key, val, new_val;
	int ret;
	uint32_t count = 0;
	uint32_t *fields;
	enum record_type type;

	mdb_cursor_open_or_die(&cursor);

	key.mv_data = (void *)sha1;
	key.mv_size = 20;
	ret = mdb_cursor_get(cursor, &key, &val, MDB_SET_KEY);
	if (ret) {
		if (ret == MDB_NOTFOUND)
			die("unknown object %s", sha1_to_hex(sha1));
		die("mdb_cursor_get failed: %s", mdb_strerror(ret));
	}

	fields = val.mv_data;

	type = ntohl(fields[0]);
	count = ntohl(fields[1]);
	count --;

	if (count == 0) {
		if (type == REC_COMMIT) {
			int i = 0;
			count = ntohl(fields[2]);
			for (i = 0; i < count; i++) {
				recursive_decref((unsigned char *)val.mv_data + 12 + i * 20);
			}
		}
		mdb_cursor_del(cursor, 0);
		return;
	}

	fields = xmalloc(val.mv_size);
	memcpy(fields, val.mv_data, val.mv_size);
	new_val.mv_size = val.mv_size;

	fields[1] = htonl(count);
	new_val.mv_data = fields;
	assert(!mdb_cursor_put(cursor, &key, &new_val, MDB_CURRENT));
	mdb_cursor_close(cursor);
	free(fields);
}

static void second_pass_object(struct object *obj);

static void second_pass_tag(struct tag *tag)
{
	incref(tag->tagged->oid.hash, 0);
	second_pass_object(tag->tagged);
}

struct sha1_array_entry *get_sha1_array_entry(const unsigned char *sha1)
{
	struct sha1_array_entry search;
	struct sha1_array_entry *found;
	hashmap_entry_init(&search, sha1hash(sha1));
	hashcpy(search.sha1, sha1);
	found = hashmap_get(&commits_by_sha, &search, NULL);
	assert(found);

	return found;
}

static void second_pass_commit(struct commit *commit)
{
	int i, n = 0;
	unsigned char *sha1;
	struct commit_list *cur;
	struct sha1_array_entry *found;
	uint64_t *my_objects = NULL;
	struct sha1_array originated = SHA1_ARRAY_INIT;
	uint64_t **parent_object_sets;
	struct sha1_array_entry **parent_entries;
	int ready = 1;

	sha1 = commit->object.oid.hash;

	found = get_sha1_array_entry(sha1);

	if (found->processed) {
		commit_stack_pop();
		return;
	}

	parse_commit_or_die(commit);

	for (cur = commit->parents; cur; cur = cur->next)
		n++;

	parent_object_sets = xmalloc(n * sizeof(*parent_object_sets));
	parent_entries = xmalloc(n * sizeof(*parent_entries));

	/*
	 * While we're processing the parents, we'd like to find one
	 * whose objectset we can reuse.
	 */
	for (i = 0, cur = commit->parents; cur; cur = cur->next) {
		struct sha1_array_entry *parent;

		parent = get_sha1_array_entry(cur->item->object.oid.hash);
		if (!parent->processed) {
			ready = 0;
			commit_stack_push(cur->item->object.oid.hash, NULL);
		}

		parent_entries[i] = parent;
		parent_object_sets[i] = parent->objectset;

		if (!my_objects && parent->children.nr == 1)
			my_objects = parent->objectset;
		i++;
	}

	if (ready)
		commit_stack_pop();
	else
		goto done;

	found->processed = 1;

	for (i = 0; i < n; i++) {
		sha1_array_append(&originated, parent_entries[i]->sha1);
		if (!my_objects) {
			my_objects = xmalloc(8 * (1 + object_nr / 64));
			memcpy(my_objects, parent_object_sets[i],
			       8 * (1 + object_nr / 64));
		} else if (my_objects != parent_object_sets[i])
			objectset_update(my_objects, parent_object_sets[i]);
	}

	if (!my_objects)
		my_objects = xcalloc(1, 8 * (1 + object_nr / 64));

	found->objectset = my_objects;

	get_originated_objects(commit, parent_object_sets, my_objects,
&originated, n);

	objectset_add_index(my_objects, objectset_index(sha1));

	for (i = 0; i < n; i++) {
		if (parent_entries[i]->children.nr == 1) {
			if (my_objects != parent_object_sets[i]) {
				free(parent_object_sets[i]);
			}
			parent_entries[i]->objectset = NULL;
			sha1_array_clear(&parent_entries[i]->children);
		} else {
			int removed = !sha1_array_remove(
				&parent_entries[i]->children, sha1);
			assert(removed);
		}
	}

	store_originated_objects(sha1, &originated);
	sha1_array_clear(&originated);

	free_commit_buffer(commit);
done:
	free(parent_entries);
	free(parent_object_sets);
}

void second_pass_finish_stack(void)
{
	while (commit_stack_nr) {
		struct object *commit = parse_object(commit_stack_top()->sha1);
		second_pass_commit((struct commit *) commit);
	}
}

static void second_pass_object(struct object *obj)
{
	if (obj->type == OBJ_TAG) {
		second_pass_tag((struct tag *)obj);
	} else if (obj->type == OBJ_COMMIT) {
		commit_stack_push(obj->oid.hash, NULL);
		second_pass_commit((struct commit *)obj);
		second_pass_finish_stack();
	}

}

static void second_pass_sha(const unsigned char sha1[20], void *data)
{
	struct object *obj = parse_object(sha1);
	if (!obj)
		return;

	incref(sha1, 0);
	second_pass_object(obj);
}

void jcdb_backfill(void)
{
	struct sha1_array roots = SHA1_ARRAY_INIT;
	struct hashmap_iter iter;
	struct sha1_array_entry *entry;

	unsigned char sha1[20];

	hashmap_init(&commits_by_sha, (hashmap_cmp_fn) sha1_array_entry_cmp, 0);
	hashmap_init(&all_objects, (hashmap_cmp_fn) sha1_entry_cmp, 0);

	for_each_ref(add_ref_to_roots, &roots);
	(void)sha1;

	/*
	 * In the first pass, we do two things:
	 *
	 * 1. We collect all object shas in the entire ODB in a big
	 * hashmap so that each one can get a small integer id.
	 * Later, we'll use these ids in bitsets.
	 *
	 * 2. We record the children of each commit.  Commits normally
	 * only store their parents, but for efficiency later, it is
	 * valuable for a commit to know when all of its children have
	 * been processed, so that we can free its active-objects
	 * bitset
	 */
	sha1_array_for_each_unique(&roots,
				   first_pass_sha,
				   NULL);
	while (commit_stack_nr) {
		const struct pending_commit *top = commit_stack_top();
		struct object *commit = parse_object(top->sha1);
		unsigned char child_sha1[20];

		hashcpy(child_sha1, top->child_sha1);

		commit_stack_pop();
		first_pass_commit((struct commit *) commit, child_sha1);
	}

	lmdb_init(dbname, 0, 1);

	/*
	 * In the second pass, we do a post-order traversal over all
	 * commits, computing the objects originated by comparing the
	 * parent object sets with the current commit objects sets.
	 */
	sha1_array_for_each_nonunique(&roots,
				      second_pass_sha,
				      NULL);

	/* Just in case there's anything left */
	second_pass_finish_stack();
	commit_stack_clear();

	hashmap_iter_init(&commits_by_sha, &iter);
	while ((entry = hashmap_iter_next(&iter))) {
		sha1_array_clear(&entry->children);
		free(entry->objectset);
	}

	hashmap_free(&commits_by_sha, 1);
	hashmap_free(&all_objects, 1);
	sha1_array_clear(&roots);

	lmdb_txn_commit();
}

static struct sha1_array needed = SHA1_ARRAY_INIT;
static struct sha1_array present = SHA1_ARRAY_INIT;

static void mark_needed(const unsigned char *sha1)
{
	sha1_array_append(&needed, sha1);
}

static void mark_present(const unsigned char *sha1)
{
	sha1_array_append(&present, sha1);
}

static void mark_commit(struct commit *commit)
{
	struct commit_list *cur;

	cur = commit->parents;
	while (cur) {
		mark_needed(cur->item->object.oid.hash);
		cur = cur->next;
	}

	mark_needed(commit->tree->object.oid.hash);
}

static int mark_needed_nonrecursive(const unsigned char *sha1,
				    struct strbuf *base,
				    const char *pathname,
				    unsigned mode, int stage,
				    void *context)
{

	if (S_ISGITLINK(mode))
		return 0;

	mark_needed(sha1);
	return 0;
}

static void mark_tree(struct tree *tree)
{
	struct pathspec pathspec;

	memset(&pathspec, 0, sizeof(pathspec));
	pathspec.nr = 0;
	pathspec.recursive = 0;
	pathspec.magic = PATHSPEC_FROMTOP;

	read_tree_recursive(tree, "", 0, 0, &pathspec,
			    mark_needed_nonrecursive, NULL);
}

static void mark_tag(struct tag *tag)
{
	mark_needed(tag->tagged->oid.hash);
}

static int is_referenced(const unsigned char *sha1, enum object_type type,
			 unsigned long size, void *data, int *eaten)
{

	struct object *obj;

	mark_present(sha1);

	if (type == OBJ_COMMIT || type == OBJ_TREE || type == OBJ_TAG) {
		obj = parse_object_buffer(sha1, type, size, data, eaten);
		if (!obj)
			die("object parsing failed");
	} else if (type == OBJ_BAD) {
		warning("bad object");
	} else  {
		return 0;
	}

	switch(type) {
	case OBJ_COMMIT:
		mark_commit((struct commit *) obj);
		break;
	case OBJ_TREE:
		mark_tree((struct tree *) obj);
		break;
	case OBJ_TAG:
		mark_tag((struct tag *) obj);
		break;
	default:
		die("BUG: should never get here");
		break;

	}

	return 0;
}

static void present_or_referenced(const unsigned char sha1[20], void *data)
{
	if (sha1_array_lookup(&present, sha1) >= 0)
		return;

	if (get_ref_count(sha1))
		return;

	die("Pack refers to %s, which is not currently referenced\n",
	    sha1_to_hex(sha1));
}

enum pack_check_result jcdb_check_and_record_pack(struct packed_git *pack)
{
	int ret;
	struct jcdb_transaction transaction;

	jcdb_transaction_begin(&transaction, JCDB_CREATE);

	if (verify_pack(pack, is_referenced, NULL, 0)) {
		jcdb_transaction_abort(&transaction);
		return PACK_INVALID;
	}

	sha1_array_for_each_unique(&needed, present_or_referenced, NULL);

	if (jcdb_add_pack(&transaction, pack->sha1))
		ret = PACK_PRESENT;
	else
		ret = PACK_ADDED;

	jcdb_transaction_commit(&transaction);
	return ret;
}

void jcdb_record_update_ref(const unsigned char *old, const unsigned char *new)
{
	/*
	 * Order of operations matters only for efficiency reasons --
	 * due to lmdb transactions, correctness will be the same
	 * either way.  We do the increments first because an update
	 * should be more efficient than a delete/recreate cycle.
	 */

	lmdb_init(dbname, 0, 1);

	if (new && !is_null_sha1(new))
		incref(new, 1);

	if (old && !is_null_sha1(old))
		recursive_decref(old);

	lmdb_txn_commit();
}

