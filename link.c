#include "cache.h"
#include "link.h"

const char *link_type = "link";

struct link *lookup_link(const unsigned char *sha1)
{
	struct object *obj = lookup_object(sha1);
	if (!obj)
		return create_object(sha1, OBJ_LINK, alloc_link_node());
	if (!obj->type)
		obj->type = OBJ_LINK;
	if (obj->type != OBJ_LINK) {
		error("Object %s is a %s, not a link",
		      sha1_to_hex(sha1), typename(obj->type));
		return NULL;
	}
	return (struct link *) obj;
}

int parse_link_buffer(struct link *item, void *buffer, unsigned long size)
{
	char *bufptr = buffer;
	char *tail = buffer + size;
	char *eol;

	if (item->object.parsed)
		return 0;
	item->object.parsed = 1;
	while (bufptr < tail) {
		eol = strchr(bufptr, '\n');
		*eol = '\0';
		if (!prefixcmp(bufptr, "name = "))
			item->upstream_url = xstrdup(bufptr + 7);
		else if (!prefixcmp(bufptr, "upstream_url = "))
			item->upstream_url = xstrdup(bufptr + 15);
		else if (!prefixcmp(bufptr, "checkout_rev = "))
			item->checkout_rev = xstrdup(bufptr + 15);
		else if (!prefixcmp(bufptr, "ref_name = "))
			item->ref_name = xstrdup(bufptr + 11);
		else if (!prefixcmp(bufptr, "floating = "))
			item->floating = atoi(bufptr + 11);
		else if (!prefixcmp(bufptr, "ignore = "))
			item->ignore = atoi(bufptr + 9);
		else
			return error("Parse error in link buffer");

		bufptr = eol + 1;
	}
	return 0;
}
