#ifndef LINK_H
#define LINK_H

#include "object.h"

extern const char *link_type;

struct link {
	struct object object;
	const char *name;
	const char *upstream_url;
	const char *checkout_rev;
	const char *ref_name;
	unsigned int floating:1;
	unsigned int ignore:2;
};

struct link *lookup_link(const unsigned char *sha1);

int parse_link_buffer(struct link *item, void *buffer, unsigned long size);

/**
 * Links do not contain references to other objects, but have
 * structured data that needs parsing.
 **/

#endif /* LINK_H */
