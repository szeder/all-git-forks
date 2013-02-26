#include "git-compat-util.h"
#include "cache.h"
#include "tree-walk.h"
#include "tree.h"
#include "unpack-trees.h"

static const char * const index_walk_usage[] = {
	"git index-walk",
	NULL
};

static struct cache_entry *next_entry_stage(struct index_state *index, int *i)
{
	struct cache_entry *ce = NULL;

	if (*i < index->cache_nr) {
		ce = index->cache[*i];
		(*i)++;
	}

	return ce;
}

void index_walk(struct index_state *index)
{
	int pos = 0;
	struct cache_entry *ce;

	while ((ce = next_entry_stage(index, &pos))) {
		printf("%06o %d%d %d %s\n",
				ce->ce_flags >> 16,
				!!(ce->ce_flags & CE_EXTENDED),
				!!(ce->ce_flags & CE_VALID),
				ce_stage(ce),
				ce->name);
	}
}

void index_walk_in_order(struct index_state *index)
{
	int i = 0;
	struct cache_entry *prev = NULL;
	char *prefix;
	
	while (i < index->cache_nr) {
		struct cache_entry *ce = index->cache[i];

		/* Note: previously processed entry: prev, initialized with
		 * NULL */

		if (!do_recurse) {
			/* skip until exit */
		}

		if (!is_prefix(prefix, ce)) {
			/* exit recursion */
			return i;
		}

		/* returns prefix/a for entry prefix/a/b/c */
		pathname = get_dirname(prefix, ce);

		if (is_dir(prefix, ce)) {
			/* TODO: May have been processed already as part of D/F
			 * conflict */
			/* TODO: May have a smaller directory after it (t-2/,
			 * t/) */

			/* enter recursion */
			yield (pathname, i);
			continue;
		}

		/* search for corresponding out of order directory entry */
		j = i+1;
		while (j < index->cache_nr) {
			/* sigh; probably better to sort... */
			if (entry e at j is greater than x for any dirname(x) smaller pathname)
			<= ...
			if (entry at j strictly greater than "pathname[0..m]/" for all m)

			if (entry at j strictly greater than pathname/) {
				/* stay at current recursion depth */
				yield (pathname);
				break;
			}
			if (is_prefix(pathname, index->cache[j])) {
				/* D/F conflict */
				/* enter recursion */
				yield (pathname with d/f conflict, j);
				break;
			}

			j++;
		}

		i++;
	}
}

int cmd_index_walk(int argc, const char **argv, const char *unused_prefix)
{
	if (argc != 1)
		die("Wrong number of arguments");

	read_cache();

	index_walk(&the_index);

	return 0;
}
