#include "cache.h"
#include "blob.h"
#include "commit.h"
#include "tag.h"
#include "tree-walk.h"
#include "permdirs.h"
#include "permdirs-walk.h"

const char *permdirs_type = "permdirs";

struct permdirs *lookup_permdirs(const unsigned char *sha1)
{
	struct object *obj = lookup_object(sha1);
	if (!obj)
		return create_object(sha1, OBJ_PERMDIRS, alloc_permdirs_node());
	if (!obj->type)
		obj->type = OBJ_PERMDIRS;
	if (obj->type != OBJ_PERMDIRS) {
		error("Object %s is a %s, not a permdirs",
		      sha1_to_hex(sha1), typename(obj->type));
		return NULL;
	}
	return (struct permdirs *) obj;
}

int parse_permdirs_buffer(struct permdirs *item, void *buffer, unsigned long size)
{
	if (item->object.parsed)
		return 0;
	item->object.parsed = 1;
	item->buffer = buffer;
	item->size = size;

	return 0;
}

int parse_permdirs(struct permdirs *item)
{
	 enum object_type type;
	 void *buffer;
	 unsigned long size;

	if (item->object.parsed)
		return 0;
	buffer = read_sha1_file(item->object.sha1, &type, &size);
	if (!buffer)
		return error("Could not read %s",
			     sha1_to_hex(item->object.sha1));
	if (type != OBJ_PERMDIRS) {
		free(buffer);
		return error("Object %s not a permdirs",
			     sha1_to_hex(item->object.sha1));
	}
	return parse_permdirs_buffer(item, buffer, size);
}

struct permdirs *parse_permdirs_indirect(const unsigned char *sha1)
{
	struct object *obj = parse_object(sha1);
	do {
		if (!obj)
			return NULL;
		if (obj->type == OBJ_PERMDIRS)
			return (struct permdirs *) obj;
		else if (obj->type == OBJ_COMMIT) {
			struct commit *commit = ((struct commit *) obj);
			if (commit->permdirs == NULL)
				return NULL;
			obj = &(commit->permdirs->object);
		} else if (obj->type == OBJ_TAG)
			obj = ((struct tag *) obj)->tagged;
		else
			return NULL;
		if (!obj->parsed)
			parse_object(obj->sha1);
	} while (1);
}
