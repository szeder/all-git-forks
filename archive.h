#ifndef ARCHIVE_H
#define ARCHIVE_H

#include "string-list.h"

struct archiver_args {
	const char *base;
	size_t baselen;
	struct tree *tree;
	const unsigned char *commit_sha1;
	const struct commit *commit;
	time_t time;
	const char **pathspec;
	unsigned int verbose : 1;
	unsigned int worktree_attributes : 1;
	int compression_level;
	struct tar_filter *tar_filter;
};

typedef int (*write_archive_fn_t)(struct archiver_args *);

typedef int (*write_archive_entry_fn_t)(struct archiver_args *args, const unsigned char *sha1, const char *path, size_t pathlen, unsigned int mode, void *buffer, unsigned long size);

/*
 * Archive-format specific backends.
 */
extern int write_tar_archive(struct archiver_args *);
extern int write_zip_archive(struct archiver_args *);
extern int write_tar_filter_archive(struct archiver_args *);

extern int write_archive_entries(struct archiver_args *args, write_archive_entry_fn_t write_entry);
extern int write_archive(int argc, const char **argv, const char *prefix, int setup_prefix, const char *name_hint);

struct tar_filter {
	char *name;
	char *command;
	struct string_list extensions;
	unsigned use_compression:1;
	struct tar_filter *next;
};

extern struct tar_filter *tar_filters;
extern struct tar_filter *tar_filter_by_name(const char *name);

extern void tar_filter_load_config(void);

const char *archive_format_from_filename(const char *filename);

#endif	/* ARCHIVE_H */
