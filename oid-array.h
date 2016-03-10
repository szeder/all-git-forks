#ifndef OID_ARRAY_H
#define OID_ARRAY_H

struct object_id;

struct oid_array {
	struct object_id *oid;
	size_t nr, alloc;
};

#define OID_ARRAY_INIT { NULL, 0, 0 }

void oid_array_init(struct oid_array *);

void oid_array_clear(struct oid_array *);

void oid_array_append(struct oid_array *, const struct object_id *);

size_t oid_array_find(const struct oid_array *, const struct object_id *);

#endif /* OID_ARRAY_H */
