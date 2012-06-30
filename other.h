#ifndef FOREIGN_H
#define FOREIGN_H

#include "object.h"

struct other_header {
	const char* key;
	const char* value;
	unsigned char sha1[20];
};

struct other {
	struct object object;
	void *util;
	char *buffer;
	char type[16]; /* up to first 15 bytes of the type */
};

extern const char *other_type;

struct other *lookup_other(const unsigned char *sha1);
int parse_other(struct other* item);
int parse_other_buffer(struct other *item, const void *buffer, unsigned long size);

/* returns -1 on error, 1 on finished, 0 on success */
int parse_other_header(char **buffer, struct other_header* hdr);

#endif
