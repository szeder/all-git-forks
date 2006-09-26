#include "cache.h"
#include "cache-tree.h"
#include "tree.h"
#include "tree-walk.h"
#include "para-walk.h"

/* test */

static void show_one(const char *type, const char *name, int namelen, const unsigned char *hash, unsigned mode)
{
	printf("%s %06o %.*s\t%.*s\n", type, mode, 7, sha1_to_hex(hash), namelen, name);
}

static const char test_para_usage[] =
"test-para [--[no-](tree|work|index)] [--] [<tree>...]";

int main(int ac, const char **av)
{
	struct para_walk w;
	unsigned char head[20];
	unsigned char trees[64][20];
	int num_tree = 0, i, using_head, show_all = 0;
	int index_wanted = 1, work_wanted = 1, tree_wanted = 0;
	const char *prefix;
	const char **pathspec;

	prefix = setup_git_directory();
	read_cache();
	get_sha1("HEAD", head);

	while (1 < ac && !strncmp(av[1], "--", 2)) {
		if (!strcmp(av[1] + 2, "all"))
			show_all = 1;
		else if (!strcmp(av[1] + 2, "tree"))
			tree_wanted = 1;
		else if (!strcmp(av[1] + 2, "work"))
			work_wanted = 1;
		else if (!strcmp(av[1] + 2, "index"))
			index_wanted = 1;
		else if (!strcmp(av[1] + 2, "no-tree"))
			tree_wanted = 0;
		else if (!strcmp(av[1] + 2, "no-work"))
			work_wanted = 0;
		else if (!strcmp(av[1] + 2, "no-index"))
			index_wanted = 0;
		else if (!av[1][2])
			break;
		else
			usage(test_para_usage);
		ac--;
		av++;
	}

	for (i = 1; i < ac; i++) {
		if (!strcmp(av[i], "--")) {
			i++;
			break; /* the rest are pathspecs */
		}
		if (get_sha1(av[i], trees[num_tree]))
			die("%s: not a valid object name", av[i]);
		num_tree++;
	}
	if (!num_tree) {
		memcpy(trees[0], head, 20);
		num_tree = 1;
		using_head = 1;
	}
	else
		using_head = 0;
	if (av[i])
		pathspec = get_pathspec(prefix, av + i);
	else
		pathspec = NULL;

	init_para_walk(&w, pathspec, index_wanted, work_wanted,
		       num_tree, trees);

	while (!extract_para_walk(&w)) {
		int i;
		int any_different = 0;
		int all_are_trees = 1;
		struct para_walk_entry *same_entry = NULL;

		for (i = 0; !any_different && i < w.num_trees + 2; i++) {
			struct para_walk_entry *e;

			if ((!index_wanted && i == 0) ||
			    (!work_wanted && i == 1))
				continue;

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

		if (show_all ||
		    (any_different && !(!tree_wanted && all_are_trees)) ) {
			/* Show them when there is a difference,
			 * except that we do not show the entry
			 * everybody is a tree but different without
			 * --tree.
			 */
			struct para_walk_entry *e;
			const char *z;

			if (!any_different) {
				e = same_entry;
				z = "same   ";
				show_one(z, e->name, e->namelen,
					 e->hash, e->mode);
			}
			else
				for (i = 0; i < w.num_trees + 2; i++) {
					char numbuf[10];

					if ((!index_wanted && i == 0) ||
					    (!work_wanted && i == 1))
						continue;

					e = &w.peek[i];
					switch (i) {
					case 0:
						z = "index  ";
						break;
					case 1:
						z = "work   ";
						break;
					default:
						if (using_head) {
							z = "HEAD   ";
						}
						else {
							sprintf(numbuf,
								"tree %2d",
								i - 1);
							z = numbuf;
						}
						break;
					}
					show_one(z, e->name, e->namelen,
						 e->hash, e->mode);
				}
		}

		if (!any_different && all_are_trees)
			skip_para_walk(&w);
		else
			update_para_walk(&w);
	}
	return 0;
}
