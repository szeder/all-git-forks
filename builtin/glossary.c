/*
 * Builtin help command
 */
#include "cache.h"
#include "builtin.h"
#include "exec_cmd.h"
#include "parse-options.h"
#include "run-command.h"
#include "column.h"
#include "glossary.h"


static int match_all = 0;
static unsigned int colopts;
static struct option builtin_glossary_options[] = {
	OPT_BOOL('a', "all", &match_all, N_("match all English git messages")),
	OPT_END(),
};

static const char * const builtin_glossary_usage[] = {
	N_("git glossary [-a|--all] [term]..."),
	NULL
};


/*
static int git_glossary_config(const char *var, const char *value, void *cb)
{
	if (starts_with(var, "column."))
		return git_column_config(var, value, "help", &colopts);

	return git_default_config(var, value, cb);
}
*/

static void emit_one(const char *one, const char* two, int pad)
{
	printf("   %s   ", one);
	for (; pad; pad--)
		putchar(' ');
	puts(two);
}

static void lookup_all(int n, const char **terms)
{
	int i;
	for (i = 0; i < n; i++)
		emit_one(terms[i], _(terms[i]), 0);
}

static void lookup_glossary(int n, const char **terms)
{
	int i, j;
	for (i = 0; i < ARRAY_SIZE(glossary); i++) {
		for (j = 0; j < n; j++) {
			if (strstr(glossary[i], terms[j]) || strstr(_(glossary[i]), terms[j])) {
				emit_one(glossary[i], _(glossary[i]), 0);
				break;
			}
		}
	}
}

static void list_glossary()
{
	int i, longest = 0;

	for (i = 0; i < ARRAY_SIZE(glossary); i++) {
		if (longest < strlen(glossary[i]))
			longest = strlen(glossary[i]);
	}

	for (i = 0; i < ARRAY_SIZE(glossary); i++)
		emit_one(glossary[i], _(glossary[i]), longest - strlen(glossary[i]));
}

int cmd_glossary(int argc, const char **argv, const char *prefix)
{
	int nongit;

	argc = parse_options(argc, argv, prefix, builtin_glossary_options,
			builtin_glossary_usage, 0);

	if (match_all && !argc) {
		printf(_("usage: %s%s"), _(builtin_glossary_usage[0]), "\n\n");
		exit(1);
	}


/*
	setup_git_directory_gently(&nongit);
	git_config(git_help_config, NULL);
*/
	if (!argc) {
		list_glossary();
		exit(0);
	}
	if (match_all)
		lookup_all(argc, argv);
	else
		lookup_glossary(argc, argv);

	return 0;
}
