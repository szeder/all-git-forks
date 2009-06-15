#include "cache.h"
#include "object.h"
#include "commit.h"
#include "diff.h"
#include "revision.h"

static int handle_walk(int argc, const char *argv[])
{
	struct commit *commit;
	struct rev_info revs;
	struct commit_list *queue, *work, **qp;
	unsigned char *sha1p, *sha1pt;
	unsigned long date = 0;
	unsigned int flags = 0;
	int slop = 5, i;
	
	init_revisions(&revs, 0);
	
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--not"))
			flags ^= UNINTERESTING;
		else if (!strcmp(argv[i], "--objects"))
			revs.tree_objects = revs.blob_objects = revs.tag_objects = 1;
		else
			handle_revision_arg(argv[i], &revs, flags, 1);
	}
	
	work = 0;
	sha1p = 0;
	for (i = 0; i < revs.pending.nr; i++) {
		commit = lookup_commit(revs.pending.objects[i].item->sha1);
		
		sha1pt = get_cache_slice(commit->object.sha1);
		if (!sha1pt)
			die("%s: not in a cache slice", sha1_to_hex(commit->object.sha1));
		
		if (!i)
			sha1p = sha1pt;
		else if (sha1p != sha1pt)
			die("walking porcelain is /per/ cache slice; commits cannot be spread out amoung several");
		
		insert_by_date(commit, &work);
	}
	
	if (!sha1p)
		die("nothing to traverse!");
	
	queue = 0;
	qp = &queue;
	commit = pop_commit(&work);
	printf("return value: %d\n", traverse_cache_slice(&revs, sha1p, commit, &date, &slop, &qp, &work));
	
	printf("queue:\n");
	while ((commit = pop_commit(&queue)) != 0) {
		printf("%s\n", sha1_to_hex(commit->object.sha1));
	}
	
	printf("work:\n");
	while ((commit = pop_commit(&work)) != 0) {
		printf("%s\n", sha1_to_hex(commit->object.sha1));
	}
	
	printf("pending:\n");
	for (i = 0; i < revs.pending.nr; i++) {
		struct object *obj = revs.pending.objects[i].item;
		
		/* unfortunately, despite our careful generation, object duplication *is* a possibility...
		 * (eg. same object introduced into two different branches) */
		if (obj->flags & SEEN)
			continue;
		
		printf("%s\n", sha1_to_hex(revs.pending.objects[i].item->sha1));
		obj->flags |= SEEN;
	}
	
	return 0;
}

static int handle_help(void)
{
	char *usage = "\
half-assed usage guide:\n\
git-rev-cache COMMAND [options] [<commit-id>...]\n\
commands:\n\
 (none) - display caches.  passing a slice hash will display detailed\n\
          information about that cache slice\n\
 walk   - walk a cache slice based on set of commits; formatted as add";
	
	puts(usage);
	
	return 0;
}

/*

usage:
git-rev-cache COMMAND [options] [<commit-id>...]
commands:
 walk		- walk a cache slice based on a given commit
   

*/

int cmd_rev_cache(int argc, const char *argv[], const char *prefix)
{
	const char *arg;
	int r;
	
	git_config(git_default_config, NULL);
	
	if (argc > 1)
		arg = argv[1];
	else
		arg = "";
	
	argc -= 2;
	argv += 2;
	if (!strcmp(arg, "add"))
		r = handle_add(argc, argv);
	else if (!strcmp(arg, "rm"))
		r = handle_rm(argc, argv);
	else if (!strcmp(arg, "walk"))
		r = handle_walk(argc, argv);
	else if (!strcmp(arg, "help"))
		return handle_help();
	else
		r = handle_show(argc, argv);
	
	printf("final return value: %d\n", r);
	
	return 0;
}
