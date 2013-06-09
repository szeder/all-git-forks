#ifndef REWRITE_H
#define REWRITE_H

struct rewritten_item {
	unsigned char from[20];
	unsigned char to[20];
};

struct rewritten {
	struct rewritten_item *items;
	unsigned int nr, alloc;
};

void add_rewritten(struct rewritten *list, unsigned char *from, unsigned char *to);
int store_rewritten(struct rewritten *list, const char *file);
void load_rewritten(struct rewritten *list, const char *file);
int run_rewrite_hook(struct rewritten *list, const char *name);
void copy_rewrite_notes(struct rewritten *list, const char *name, const char *msg);

#endif
