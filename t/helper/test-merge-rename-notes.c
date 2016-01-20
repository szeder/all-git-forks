#include "cache.h"
#include "diff.h"
#include "diffcore.h"
#include "notes.h"
#include "notes-utils.h"

int cmd_main(int ac, const char **av)
{
	struct notes_tree rename_notes;
	struct notes_tree cache;
	const char *ref = av[1];
	const char *cache_ref = av[2];

	memset(&rename_notes, 0, sizeof(rename_notes));
	init_notes(&rename_notes, ref, NULL, 0);
	init_notes(&cache, cache_ref, NULL, 0);
	merge_rename_notes(&rename_notes, &cache);
	commit_notes(&cache, "update");
	return 0;
}
