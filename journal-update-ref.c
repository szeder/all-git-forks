#include "cache.h"
#include "journal-update-ref.h"
#include "hashmap.h"
#include "refs.h"

struct ref_op_ctx {
	struct hashmap map;
};

static int ref_map_entry_cmp_icase(const struct ref_map_entry *e1,
				   const struct ref_map_entry *e2,
				   const char* key)
{
	return strcasecmp(e1->name, key ? key : e2->name);
}

static struct ref_map_entry *ref_map_entry_alloc(int hash, const char *key,
						 int klen,
						 const unsigned char *sha)
{
	struct ref_map_entry *entry;
	entry = xmalloc(sizeof(*entry) + klen + 1 /* trailing \0 */ + 20 /* sha */);
	hashmap_entry_init(entry, hash);
	memcpy(entry->name, key, klen + 1);
	hashcpy((unsigned char *)entry->sha1, sha);
	return entry;
}

void ref_op_add(struct ref_op_ctx *ctx, const char *name, size_t name_len,
		const unsigned char *sha1)
{
	struct ref_map_entry *entry;
	/* Set up key */
	const int hash = memihash(name, name_len);
	struct hashmap_entry key;
	hashmap_entry_init(&key, hash);

	/* Find */
	entry = hashmap_get(&ctx->map, &key, name);

	if (entry) {
		/* Replace existing value */
		hashcpy(entry->sha1, sha1);
	} else {
		/* Create new value */
		entry = ref_map_entry_alloc(hash, name, name_len, sha1);
		hashmap_add(&ctx->map, entry);
	}
}

struct ref_map_entry *ref_op_remove(struct ref_op_ctx *ctx, const char *name, size_t name_len)
{
	/* Set up key */
	const int hash = memihash(name, name_len);
	struct hashmap_entry key;
	hashmap_entry_init(&key, hash);

	return hashmap_remove(&ctx->map, &key, name);
}


static const char *ref_op_kind_get_verb(enum ref_op_kind kind) {
	if (kind == REF_OP_KIND_CREATE)
		return "create";
	else if (kind == REF_OP_KIND_UPDATE)
		return "update";
	else if (kind == REF_OP_KIND_DELETE)
		return "delete";
	else
		return "unkown";
}

static enum ref_op_kind ref_map_entry_get_kind(struct ref_map_entry *e)
{
	const char *name = e->name;
	const unsigned char *primary_sha = e->sha1;

	if (is_null_sha1(primary_sha))
		return REF_OP_KIND_DELETE;
	else if (ref_exists(name))
		return REF_OP_KIND_UPDATE;
	else
		return REF_OP_KIND_CREATE;
}

void ref_map_entry_to_strbuf(struct ref_map_entry *e, struct strbuf *s,
		char delim)
{
	const char *verb;
	const char *name = e->name;
	const unsigned char *primary_sha = e->sha1;
	enum ref_op_kind kind = ref_map_entry_get_kind(e);

	verb = ref_op_kind_get_verb(kind);
	strbuf_addstr(s, verb);
	strbuf_addch(s, ' ');
	strbuf_addstr(s, name);
	strbuf_addch(s, delim);
	switch (kind) {
	case REF_OP_KIND_CREATE:
		strbuf_addstr(s, sha1_to_hex(primary_sha));
		break;
	case REF_OP_KIND_UPDATE:
		strbuf_addstr(s, sha1_to_hex(primary_sha));
		strbuf_addch(s, delim);
		break;
	case REF_OP_KIND_DELETE:
	default:
		break;
	}
}

struct ref_op_ctx *ref_op_ctx_create(void)
{
	struct ref_op_ctx *c;
	c = xcalloc(1, sizeof(*c));
	hashmap_init(&c->map, (hashmap_cmp_fn) ref_map_entry_cmp_icase, 0);
	return c;
}

void ref_op_ctx_destroy(struct ref_op_ctx *ctx)
{
	hashmap_free(&ctx->map, 0);
	free(ctx);
}

int ref_op_ctx_each_op(struct ref_op_ctx *ctx, enum ref_op_kind kinds,
		ref_op_ctx_each_op_cb cb, void *data)
{
	int ret = 0;
	struct hashmap_iter iter;
	struct ref_map_entry *entry;
	hashmap_iter_init(&ctx->map, &iter);
	while ((entry = hashmap_iter_next(&iter))) {
		const enum ref_op_kind kind = ref_map_entry_get_kind(entry);
		if (kinds & kind) {
			ret = cb(ctx, entry, data);
			trace_printf("%s -> %s = %d\n", entry->name, sha1_to_hex(entry->sha1), ret);
			if (ret != 0)
				break;
		}
	}

	return ret;
}
