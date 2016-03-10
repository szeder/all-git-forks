#ifndef REBASE_COMMON_H
#define REBASE_COMMON_H

/**
 * Refresh and write index.
 */
void refresh_and_write_cache(unsigned int);

/**
 * Returns 1 if there are unstaged changes, 0 otherwise.
 */
int cache_has_unstaged_changes(void);

/**
 * Returns 1 if there are uncommitted changes, 0 otherwise.
 */
int cache_has_uncommitted_changes(void);

/* common rebase backend options */
struct rebase_options {
	struct object_id onto;
	char *onto_name;

	struct object_id upstream;

	struct object_id orig_head;
	char *orig_refname;

	const char *resolvemsg;
};

void rebase_options_init(struct rebase_options *);

void rebase_options_release(struct rebase_options *);

void rebase_options_swap(struct rebase_options *dst, struct rebase_options *src);

int rebase_options_load(struct rebase_options *, const char *dir);

void rebase_options_save(const struct rebase_options *, const char *dir);

void rebase_common_setup(struct rebase_options *, const char *dir);

void rebase_common_destroy(struct rebase_options *, const char *dir);

void rebase_common_finish(struct rebase_options *, const char *dir);

#endif /* REBASE_COMMON_H */
