#include "cache.h"
#include "cache-tree.h"
#include "tree.h"
#include "tree-walk.h"
#include "para-walk.h"
#include "commit.h"
#include "diff.h"
#include "revision.h"

int main(int ac, const char **av)
{
	struct para_walk w;
	unsigned char head[1][20];
	const char *prefix;
	const char **pathspec;
	struct combine_diff_path *cdp;

	prefix = setup_git_directory();
	read_cache();
	get_sha1("HEAD", head[0]);

	if (2 <= ac)
		pathspec = get_pathspec(prefix, av + 1);
	else
		pathspec = NULL;

	cdp = xcalloc(1, combine_diff_path_size(2, 0));

	init_para_walk(&w, pathspec, 1, 1, 1, head);
	while (!extract_para_walk(&w)) {
		int i;
		int any_different = 0;
		int all_are_trees = 1;
		struct para_walk_entry *same_entry = NULL;

		for (i = 0; !any_different && i < w.num_trees + 2; i++) {
			struct para_walk_entry *e;
			e = &w.peek[i];
			if (!same_entry)
				same_entry = e;
			else if (e->mode == same_entry->mode &&
				 !is_null_sha1(e->hash) &&
				 !hashcmp(e->hash, same_entry->hash))
				;
			else
				any_different = 1;

			if (!e->mode || !S_ISDIR(e->mode))
				all_are_trees = 0;
		}

		if ((any_different && !all_are_trees)) {
			/* Show them when there is a difference,
			 * except that we do not show the entry
			 * everybody is a tree but different without
			 * --tree.
			 */
			if (any_different) {
				struct rev_info rev;
				memset(&rev, 0, sizeof(rev));
				rev.diffopt.output_format = DIFF_FORMAT_PATCH;

				cdp->path = strdup(w.peek[0].name);
				cdp->len = w.peek[0].namelen;

				/*
				 * treat HEAD (peek[2]) and index (peek[0])
				 * as parents of work tree (peek[1]) and
				 * run combined diff.
				 */
				cdp->mode = w.peek[1].mode;
				hashcpy(cdp->sha1, w.peek[1].hash);

				/* HEAD */
				cdp->parent[0].mode = w.peek[2].mode;
				hashcpy(cdp->parent[0].sha1, w.peek[2].hash);
				/* index */
				cdp->parent[1].mode = w.peek[0].mode;
				hashcpy(cdp->parent[1].sha1, w.peek[0].hash);

				show_combined_diff(cdp, 2, 0, &rev);
				free(cdp->path);
			}
		}

		if (!any_different && all_are_trees)
			skip_para_walk(&w);
		else
			update_para_walk(&w);
	}
	return 0;
}
