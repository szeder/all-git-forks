#ifndef PERMDIRS_H
#define PERMDIRS_H

#include "object.h"

extern const char *permdirs_type;

struct permdirs {
	struct object object;
	void *buffer;
	unsigned long size;
};

struct permdirs *lookup_permdirs(const unsigned char *sha1);

int parse_permdirs_buffer(struct permdirs *item, void *buffer, unsigned long size);

int parse_permdirs(struct permdirs *permdirs);

/* Parses and returns the permdirs in the given ent, chasing tags and commits. */
struct permdirs *parse_permdirs_indirect(const unsigned char *sha1);

extern int read_permdirs_recursive(struct permdirs *permdirs,
			       const char *base, int baselen,
			       int stage, struct pathspec *pathspec,
			       read_tree_fn_t fn, void *context);

extern int read_permdirs(struct permdirs *permdirs, int stage, struct pathspec *pathspec);

#endif /* PERMDIRS_H */
