#include "cache.h"
#include "rebase-common.h"
#include "lockfile.h"
#include "revision.h"

void refresh_and_write_cache(unsigned int flags)
{
	struct lock_file *lock_file = xcalloc(1, sizeof(struct lock_file));

	hold_locked_index(lock_file, 1);
	refresh_cache(flags);
	if (write_locked_index(&the_index, lock_file, COMMIT_LOCK))
		die(_("unable to write index file"));
}

int cache_has_unstaged_changes(void)
{
	struct rev_info rev_info;
	int result;

	init_revisions(&rev_info, NULL);
	DIFF_OPT_SET(&rev_info.diffopt, IGNORE_SUBMODULES);
	DIFF_OPT_SET(&rev_info.diffopt, QUICK);
	diff_setup_done(&rev_info.diffopt);
	result = run_diff_files(&rev_info, 0);
	return diff_result_code(&rev_info.diffopt, result);
}

int cache_has_uncommitted_changes(void)
{
	struct rev_info rev_info;
	int result;

	if (is_cache_unborn())
		return 0;

	init_revisions(&rev_info, NULL);
	DIFF_OPT_SET(&rev_info.diffopt, IGNORE_SUBMODULES);
	DIFF_OPT_SET(&rev_info.diffopt, QUICK);
	add_head_to_pending(&rev_info);
	diff_setup_done(&rev_info.diffopt);
	result = run_diff_index(&rev_info, 1);
	return diff_result_code(&rev_info.diffopt, result);
}
