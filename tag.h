#ifndef TAG_H
#define TAG_H

#include "object.h"

extern const char *tag_type;

struct tag {
	struct object object;
	struct object *tagged;
	char *tag;       /* optional, may be empty ("") */
	char *keywords;  /* optional, defaults to tag ? "tag" : "note" */
	char *signature; /* not actually implemented */
};

extern struct tag *lookup_tag(const unsigned char *sha1);
extern int parse_and_verify_tag_buffer(struct tag *item, const char *data, const unsigned long size, int thorough_verify);
extern int parse_tag_buffer(struct tag *item, void *data, unsigned long size);
extern int parse_tag(struct tag *item);
extern struct object *deref_tag(struct object *, const char *, int);

#endif /* TAG_H */
