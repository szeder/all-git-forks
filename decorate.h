#ifndef DECORATE_H
#define DECORATE_H

struct object_decoration {
	unsigned char sha1[20];
	void *decoration;
};

struct decoration {
	const char *name;
	unsigned int size, nr;
	struct object_decoration *hash;
};

extern void *add_decoration(struct decoration *n, const unsigned char *sha1, void *decoration);
extern void *lookup_decoration(struct decoration *n, const unsigned char *sha1);

#endif
