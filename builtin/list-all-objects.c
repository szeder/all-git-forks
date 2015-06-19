#include "cache.h"
#include "builtin.h"
#include "revision.h"
#include "parse-options.h"

#include <stdio.h>

static int verbose;

static int print_object(const unsigned char *sha1)
{
	if (verbose) {
		unsigned long size;
		int type = sha1_object_info(sha1, &size);

		if (type < 0)
			return -1;

		printf("%s %s %lu\n", sha1_to_hex(sha1), typename(type), size);
	}
	else
		printf("%s\n", sha1_to_hex(sha1));

	return 0;
}

static int check_loose_object(const unsigned char *sha1,
			      const char *path,
			      void *data)
{
	return print_object(sha1);
}

static int check_packed_object(const unsigned char *sha1,
			       struct packed_git *pack,
			       uint32_t pos,
			       void *data)
{
	return print_object(sha1);
}

static struct option builtin_filter_objects_options[] = {
	OPT__VERBOSE(&verbose, "show object type and size"),
	OPT_END()
};

int cmd_list_all_objects(int argc, const char **argv, const char *prefix)
{
	struct packed_git *p;

	argc = parse_options(argc, argv, prefix, builtin_filter_objects_options,
			     NULL, 0);

	for_each_loose_object(check_loose_object, NULL, 0);

	prepare_packed_git();
	for (p = packed_git; p; p = p->next) {
		open_pack_index(p);
	}

	for_each_packed_object(check_packed_object, NULL, 0);

	return 0;
}
