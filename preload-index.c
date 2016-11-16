/*
 * Copyright (C) 2008 Linus Torvalds
 */
#include "cache.h"
#include "pathspec.h"
#include "dir.h"

#ifdef NO_PTHREADS
static void preload_index(struct index_state *index,
			  const struct pathspec *pathspec)
{
	; /* nothing */
}
#else

#include <pthread.h>

/*
 * Mostly randomly chosen maximum thread counts: we
 * cap the parallelism to 20 threads, and we want
 * to have at least 500 lstat's per thread for it to
 * be worth starting a thread.
 */
#define MAX_PARALLEL (20)
#define THREAD_COST (500)

struct thread_data {
	pthread_t pthread;
	struct index_state *index;
	struct pathspec pathspec;
	int offset, nr;

	int sum_already_known_skipped;
	int sum_try_path_match;
	int sum_try_symlink_leading_path;
	int sum_try_lstat;
	int sum_try_match_stat;
	int sum_mark_uptodate;
	uint64_t thread_time;
	DWORD dw_thread_id;
};

extern int fscache_sum_take_mutex;
extern int fscache_sum_create_event;

/*
 * Precompute the hash values for this cache_entry
 * for use in the istate.name_hash and istate.dir_hash.
 *
 * If the item is in the root directory, just compute the
 * hash value (for istate.name_hash) on the full path.
 *
 * If the item is in a subdirectory, first compute the
 * hash value for the immediate parent directory (for
 * istate.dir_hash) and then the hash value for the full
 * path by continuing the computation.
 *
 * Note that these hashes will be used by
 * wt_status_collect_untracked() as it scans the worktree
 * and maps observed paths back to the index (optionally
 * ignoring case).  Therefore, we probably only *NEED* to
 * precompute this for non-skip-worktree items (since
 * status should not observe skipped items), but because
 * lazy_init_name_hash() hashes everything, we force it
 * here.
 */ 
static void precompute_istate_hashes(struct cache_entry *ce)
{
	int namelen = ce_namelen(ce);

	while (namelen > 0 && !is_dir_sep(ce->name[namelen - 1]))
		namelen--;
	if (namelen <= 0) {
		ce->preload_hash_name = memihash(ce->name, ce_namelen(ce));
		ce->preload_hash_state = CE_PRELOAD_HASH_STATE__SET;
	} else {
		namelen--;
		ce->preload_hash_dir = memihash(ce->name, namelen);
		ce->preload_hash_name = memihash2(
			ce->preload_hash_dir, &ce->name[namelen],
			ce_namelen(ce) - namelen);
		ce->preload_hash_state =
			CE_PRELOAD_HASH_STATE__SET | CE_PRELOAD_HASH_STATE__DIR;
#if 0
		trace_printf("precompute: [0x%08x,0x%08x] [0x%08x]\n",
					 ce->preload_hash_dir,
					 ce->preload_hash_name,
					 memihash(ce->name, ce_namelen(ce)));
#endif

	}
}

static void *preload_thread(void *_data)
{
	int nr;
	struct thread_data *p = _data;
	struct index_state *index = p->index;
	struct cache_entry **cep = index->cache + p->offset;
	struct cache_def cache = CACHE_DEF_INIT;

	p->dw_thread_id = GetCurrentThreadId();

	uint64_t start_time = getnanotime();

	nr = p->nr;
	if (nr + p->offset > index->cache_nr)
		nr = index->cache_nr - p->offset;

	do {
		struct cache_entry *ce = *cep++;
		struct stat st;

		precompute_istate_hashes(ce);

		if (ce_stage(ce))
			continue;
		if (S_ISGITLINK(ce->ce_mode))
			continue;
		if (ce_uptodate(ce))
			continue;

		if (ce_skip_worktree(ce)) {
			p->sum_already_known_skipped++;
			continue;
		}

		p->sum_try_path_match++;
		if (!ce_path_match(ce, &p->pathspec, NULL))
			continue;

		p->sum_try_symlink_leading_path++;
		if (threaded_has_symlink_leading_path(&cache, ce->name, ce_namelen(ce)))
			continue;

		p->sum_try_lstat++;
		if (lstat(ce->name, &st))
			continue;

		p->sum_try_match_stat++;
		if (ie_match_stat(index, ce, &st, CE_MATCH_RACY_IS_DIRTY))
			continue;

		p->sum_mark_uptodate++;
		ce_mark_uptodate(ce);
	} while (--nr > 0);
	cache_def_clear(&cache);

	p->thread_time = getnanotime() - start_time;

	return NULL;
}

static void preload_index(struct index_state *index,
			  const struct pathspec *pathspec)
{
	int threads, i, work, offset;
	struct thread_data data[MAX_PARALLEL];

	uint64_t start_time = getnanotime();

	if (!core_preload_index)
		return;

	threads = index->cache_nr / THREAD_COST;
	if (threads < 2)
		return;
	if (threads > MAX_PARALLEL)
		threads = MAX_PARALLEL;
	offset = 0;
	work = DIV_ROUND_UP(index->cache_nr, threads);
	memset(&data, 0, sizeof(data));
	enable_fscache(1);
	for (i = 0; i < threads; i++) {
		struct thread_data *p = data+i;
		p->index = index;
		if (pathspec)
			copy_pathspec(&p->pathspec, pathspec);
		p->offset = offset;
		p->nr = work;
		offset += work;
		if (pthread_create(&p->pthread, NULL, preload_thread, p))
			die("unable to create threaded lstat");
	}
	for (i = 0; i < threads; i++) {
		struct thread_data *p = data+i;
		if (pthread_join(p->pthread, NULL))
			die("unable to join threaded lstat");

		trace_performance(p->thread_time,
						  "    preload[0x%08x] [%02d]: [known_skipped %d][pathmatch %d][symlink %d][lstat %d][match_stat %d][mark_uptodate %d]",
						  p->dw_thread_id,
						  i,
						  p->sum_already_known_skipped,
						  p->sum_try_path_match,
						  p->sum_try_symlink_leading_path,
						  p->sum_try_lstat,
						  p->sum_try_match_stat,
						  p->sum_mark_uptodate);
	}
	enable_fscache(0);

	trace_performance_since(start_time, "preload_index: fscache [mutex %d][events %d]",
							fscache_sum_take_mutex, fscache_sum_create_event);
}
#endif

int read_index_preload(struct index_state *index,
		       const struct pathspec *pathspec)
{
	uint64_t start_time = getnanotime();
	int retval = read_index(index);
	trace_performance_since(start_time, "read_index");

	preload_index(index, pathspec);
	return retval;
}
