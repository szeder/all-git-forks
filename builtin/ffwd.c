#include "builtin.h"
#include "parse-options.h"
#include "strbuf.h"
#include "string-list.h"
#include "remote.h"
#include "argv-array.h"
#include "run-command.h"
#include "ffwd.h"

static int verbosity;

static int get_one_remote_for_fetch(struct remote *remote, void *priv)
{
	struct string_list *list = priv;
	if (!remote->skip_default_update)
		string_list_append(list, remote->name);
	return 0;
}

struct remote_group_data {
	const char *name;
	struct string_list *list;
};

static int get_remote_group(const char *key, const char *value, void *priv)
{
	struct remote_group_data *g = priv;

	if (!prefixcmp(key, "remotes.") &&
			!strcmp(key + 8, g->name)) {
		/* split list by white space */
		int space = strcspn(value, " \t\n");
		while (*value) {
			if (space > 1) {
				string_list_append(g->list,
						   xstrndup(value, space));
			}
			value += space + (value[space] != '\0');
			space = strcspn(value, " \t\n");
		}
	}

	return 0;
}

static int add_remote_or_group(const char *name, struct string_list *list)
{
	int prev_nr = list->nr;
	struct remote_group_data g;
	g.name = name; g.list = list;

	git_config(get_remote_group, &g);
	if (list->nr == prev_nr) {
		struct remote *remote;
		if (!remote_is_configured(name))
			return 0;
		remote = remote_get(name);
		string_list_append(list, remote->name);
	}
	return 1;
}

static int truncate_fetch_head(void)
{
	char *filename = git_path("FETCH_HEAD");
	FILE *fp = fopen(filename, "w");

	if (!fp)
		return error(_("cannot open %s: %s\n"), filename, strerror(errno));
	fclose(fp);
	return 0;
}

static void add_options_to_argv(struct argv_array *argv)
{
	argv_array_push(argv, "--prune");

	// Enable this once it no longer prunes tags.
	/* argv_array_push(argv, "--tags"); */
	if (verbosity >= 2)
		argv_array_push(argv, "-v");
	if (verbosity >= 1)
		argv_array_push(argv, "-v");
	else if (verbosity < 0)
		argv_array_push(argv, "-q");
}

static int fetch_multiple(struct string_list *list)
{
	int i, result = 0;
	struct argv_array argv = ARGV_ARRAY_INIT;

	if (1) {
		int errcode = truncate_fetch_head();
		if (errcode)
			return errcode;
	}

	argv_array_pushl(&argv, "fetch", "--append", NULL);
	add_options_to_argv(&argv);

	for (i = 0; i < list->nr; i++) {
		const char *name = list->items[i].string;
		argv_array_push(&argv, name);
		if (verbosity >= 0)
			printf(_("Fetching %s\n"), name);
		if (run_command_v_opt(argv.argv, RUN_GIT_CMD)) {
			error(_("Could not fetch %s"), name);
			result = 1;
		}
		argv_array_pop(&argv);
	}

	argv_array_clear(&argv);
	return result;
}

int cmd_ffwd(int argc, const char **argv, const char *prefix)
{
	struct string_list list = STRING_LIST_INIT_NODUP;
	int result = 0;
	int prune = -1;
	struct option local_opts[] = {
		OPT_BOOL('p', "prune", &prune,
			 N_("prune remote-tracking branches no longer on remote")),
		OPT_END()
	};
	int i;

	argc = parse_options(argc, argv, prefix, local_opts, NULL, 0);

	if (argc == 0) {
		/* No arguments -- fetch all */
		(void) for_each_remote(get_one_remote_for_fetch, &list);
		/* result = fetch_multiple(&list); */
	} else {
		/* All arguments are assumed to be remotes or groups */
		for (i = 0; i < argc; i++)
			if (!add_remote_or_group(argv[i], &list))
				die(_("No such remote or remote group: %s"), argv[i]);
		/* result = fetch_multiple(&list); */
	}

	return ffwd(&list);
}

