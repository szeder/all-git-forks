#include "builtin.h"
#include "authors.h"
#include "parse-options.h"

static const char *const builtin_authors_usage[] = {
	N_("git authors [<options>]"),
	NULL
};

static int actions;

#define ACTION_LIST (1<<0)
#define ACTION_GET (1<<1)
#define ACTION_SET (1<<2)
#define ACTION_CLEAR (1<<3)

static struct option builtin_authors_options[] = {
	OPT_BIT('c', "clear", &actions, N_("clear current authors"), ACTION_CLEAR),
	OPT_BIT('l', "list", &actions, N_("list all available authors"), ACTION_LIST)
};

static struct string_list authors_map = STRING_LIST_INIT_NODUP;

int cmd_authors(int argc, const char **argv, const char *prefix)
{
	struct string_list_item *item;

	argc = parse_options(argc, argv, prefix, builtin_authors_options,
			     builtin_authors_usage,
			     PARSE_OPT_STOP_AT_NON_OPTION);

	if (actions == 0) {
	if (argc == 0)
		actions = ACTION_GET;
	else
	actions = ACTION_SET;
	}

	read_authors_map_file(&authors_map);

	if (actions == ACTION_LIST)
		for_each_string_list_item(item, &authors_map) {
		printf("%s\n", item->string);
	}
	else if (actions == ACTION_GET) {
		const char *authors_config = NULL;
		const char *expanded_authors;

		if (git_config_get_string_const("authors.current", &authors_config))
			die("No current authors set. Use `git authors <initials> <initials> to set authors.");

		printf("Short:    %s\n", authors_config);

		expanded_authors = expand_authors(&authors_map, authors_config);

		printf("Expanded: %s\n", expanded_authors);
	}
	else if (actions == ACTION_SET) {
		int i;
		static struct strbuf authors_info = STRBUF_INIT;
		int lookup_error = 0;

		for (i = 0; i < argc; ++i) {
			if (!lookup_author(&authors_map, argv[i])) {
				lookup_error--;
				error("Couldn't find author '%s'", argv[i]);
			}
			if (i > 0)
				strbuf_addch(&authors_info, ' ');
				strbuf_addstr(&authors_info, argv[i]);
		}
		if (lookup_error < 0)
			die("Add missing authors to ~/.git_authors_map");

		git_config_set("authors.current", authors_info.buf);
	}
	else if (actions == ACTION_CLEAR) {
		git_config_set("authors.current", NULL);
	}

	return 0;
}
