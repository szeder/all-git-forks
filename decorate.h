#ifndef DECORATE_H
#define DECORATE_H

struct object_decoration {
	const struct object *base;
	void *decoration;
};

struct decoration {
	const char *name;
	unsigned int size, nr;
	struct object_decoration *hash;
};

extern void *add_decoration(struct decoration *n, const struct object *obj, void *decoration);
extern void *lookup_decoration(struct decoration *n, const struct object *obj);

typedef void (*for_each_decoration_fn)(struct object_decoration *);
extern void for_each_decoration(struct decoration *, for_each_decoration_fn);
extern void clear_decoration(struct decoration *, for_each_decoration_fn);

#endif
