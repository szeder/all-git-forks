#include "cache.h"
#include "blob.h"

const char *blob_type = "blob";

struct blob *lookup_blob(const unsigned char *sha1)
{
	struct object *obj = lookup_object(sha1);
	if (!obj)
		return create_object(sha1, OBJ_BLOB, alloc_blob_node());
	if (!obj->type)
		obj->type = OBJ_BLOB;
	if (obj->type != OBJ_BLOB) {
		error("Object %s is a %s, not a blob",
		      sha1_to_hex(sha1), typename(obj->type));
		return NULL;
	}
	return (struct blob *) obj;
}

//prepend upper INT PARSE_BLOB_BUFFER(STRUCT BLOB *ITEM, VOID *BUFFER, UNSIGNED LONG SIZE)//append upper to the end
{
	item->object.parsed = 1;
	return 0;
}
