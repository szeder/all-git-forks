#ifndef REBASE_COMMON_H
#define REBASE_COMMON_H
#include "argv-array.h"

struct object_id;
struct child_process;

/**
 * Refresh and write cache. Returns 0 on success, -1 on failure.
 */
int refresh_and_write_cache(unsigned int);

/**
 * Returns 1 if there are unstaged changes, 0 otherwise.
 */
int cache_has_unstaged_changes(int);

/**
 * Returns 1 if there are uncommitted changes, 0 otherwise.
 */
int cache_has_uncommitted_changes(int);

/**
 * If the work tree has unstaged or uncommitted changes, die() with the
 * appropriate message.
 */
void rebase_die_on_unclean_worktree(int);

int reset_hard(const struct object_id *);

int copy_notes_for_rebase(const char *);

/* common rebase backend options */
struct rebase_options {
	struct object_id onto;
	char *onto_name;

	struct object_id upstream;

	struct object_id orig_head;
	char *orig_refname;

	int quiet;
	int verbose;
	char *strategy;
	struct argv_array strategy_opts;
	char *allow_rerere_autoupdate;
	char *gpg_sign_opt;
	const char *resolvemsg;
	int force;
	int root;

	int autostash;
};

void rebase_options_init(struct rebase_options *);

void rebase_options_release(struct rebase_options *);

void rebase_options_swap(struct rebase_options *dst, struct rebase_options *src);

int rebase_options_load(struct rebase_options *, const char *);

void rebase_options_save(const struct rebase_options *, const char *);

int rebase_common_setup(struct rebase_options *, const char *);

int rebase_output(const struct rebase_options *, struct child_process *);

void rebase_move_to_original_branch(struct rebase_options *);

void rebase_common_finish(struct rebase_options *, const char *);

#endif /* REBASE_COMMON_H */
