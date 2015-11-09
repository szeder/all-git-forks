#include "../cache.h"
#include "../cache-tree.h"
#include "../diff.h"
#include "../diffcore.h"
#include "../revision.h"

static const char builtin_stash__helper_usage[] = {
	"Usage: git stash--helper --non-patch <i_tree>"
};

static void diff_callback(struct diff_queue_struct *q,
					 struct diff_options *options, void *data)
{
	int i;
	for (i = 0; i < q->nr; i++)
    {
		struct diff_filepair *p;
		struct stat st;

		p = q->queue[i];
		if (p->one && lstat(p->one->path, &st)) {
			warning("Could not stat %s", p->one->path);
			continue;
		}
		add_to_index(&the_index, p->one->path, &st, 0);
	}
}

int stash_non_patch(const char *i_tree,	const char *prefix)
{
	int result;
	struct rev_info rev;
	const char *wt_prefix = NULL;
	unsigned char sha1[20];
	const char *me = "git stash--helper";

	read_index(&the_index);
    refresh_cache(REFRESH_QUIET);
	init_revisions(&rev, NULL);
	add_head_to_pending(&rev);
	rev.diffopt.output_format |= DIFF_FORMAT_CALLBACK;
	rev.diffopt.format_callback = diff_callback;
	rev.diffopt.format_callback_data=&result;
	diff_setup_done(&rev.diffopt);
    run_diff_index(&rev,0);
    result = write_cache_as_tree(sha1, 0, wt_prefix);

	switch (result) {
	case 0:
		printf("%s\n", sha1_to_hex(sha1));
		break;
	case WRITE_TREE_UNREADABLE_INDEX:
		die("%s: error reading the index", me);
		break;
	case WRITE_TREE_UNMERGED_INDEX:
		die("%s: error building trees", me);
		break;
	case WRITE_TREE_PREFIX_ERROR:
		die("%s: prefix %s not found", me, wt_prefix);
		break;
	}
	return result;
}

int cmd_stash__helper(int argc, const char **argv, const char *prefix)
{
	if (argc == 3 && !strcmp("--non-patch", argv[1]))
		return stash_non_patch(argv[2], prefix);
	usage(builtin_stash__helper_usage);
}
