#ifndef __ENTRY_H__
#define __ENTRY_H__

struct progress;

struct checkout {
	struct index_state *istate;
	const char *base_dir;
	int base_dir_len;
	unsigned force:1,
		 quiet:1,
		 not_new:1,
		 refresh_cache:1;
};

#define TEMPORARY_FILENAME_LENGTH 25
extern int checkout_entry(struct cache_entry *ce,
			  const struct checkout *state,
			  char *topath);

int start_parallel_checkout(const struct checkout *state);
int parallel_checkout_worker(void);
int run_parallel_checkout(int nr_workers, int min_limit,
			  int progress_count, struct progress *progress);

#endif
