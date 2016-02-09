#pragma once
#include "strbuf.h"

struct ref_op_ctx;
struct ref_op;
struct ref_map_entry;

enum ref_op_kind {
	REF_OP_KIND_CREATE = (1 << 0),
	REF_OP_KIND_UPDATE = (1 << 1),
	REF_OP_KIND_DELETE = (1 << 2)
};

struct ref_map_entry {
	struct hashmap_entry ent;
	unsigned char sha1[20];
	char name[FLEX_ARRAY];
};

struct ref_op_ctx *ref_op_ctx_create(void);
void ref_op_ctx_destroy(struct ref_op_ctx *ctx);

typedef int (*ref_op_ctx_each_op_cb)(struct ref_op_ctx *ctx, struct ref_map_entry *e, void *data);
int ref_op_ctx_each_op(struct ref_op_ctx *ctx, enum ref_op_kind kinds, ref_op_ctx_each_op_cb cb, void *data);
void ref_op_add(struct ref_op_ctx *ctx, const char *name, size_t name_len,
		const unsigned char *sha);
struct ref_map_entry *ref_op_remove(struct ref_op_ctx *ctx, const char *name,
				    size_t name_len);

void ref_map_entry_to_strbuf(struct ref_map_entry *e, struct strbuf *s,
			     char delim);

void ref_entry_to_strbuf(struct ref_map_entry *e, struct strbuf *s,
			 char delim, char term);
