#ifndef PERMDIRS_WALK_H
#define PERMDIRS_WALK_H

struct permdirs_desc {
	const void *buffer;
	struct name_entry entry;
	unsigned int size;
};

static inline const char *permdirs_entry_extract(struct permdirs_desc *desc)
{
	return desc->entry.path;
}

static inline int permdirs_entry_len(const struct name_entry *ne)
{
	return strlen(ne->path);
}

void update_permdirs_entry(struct permdirs_desc *);
void init_permdirs_desc(struct permdirs_desc *desc, const void *buf, unsigned long size);

/*
 * Helper function that does both permdirs_entry_extract() and update_permdirs_entry()
 * and returns true for success
 */
int permdirs_entry(struct permdirs_desc *, struct name_entry *);

void *fill_permdirs_descriptor(struct permdirs_desc *desc, const unsigned char *sha1);

struct traverse_info;
int traverse_permdirs(int n, struct permdirs_desc *t, struct traverse_info *info);

int get_permdirs_entry(const unsigned char *, const char *);
extern char *make_traverse_path(char *path, const struct traverse_info *info, const struct name_entry *n);
extern void setup_traverse_info(struct traverse_info *info, const char *base);

extern enum interesting permdirs_entry_interesting(const struct name_entry *,
					       struct strbuf *, int,
					       const struct pathspec *ps);

#endif
