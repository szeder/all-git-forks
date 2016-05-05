#include "cache.h"
#include "parse-options.h"
#include "string-list.h"
#include "strbuf.h"

static int boolean = 0;
static int integer = 0;
static unsigned long magnitude = 0;
static unsigned long timestamp;
static int abbrev = 7;
static int verbose = -1; /* unspecified */
static int dry_run = 0, quiet = 0;
static char *string = NULL;
static char *file = NULL;
static int ambiguous;
static struct string_list list;

static struct {
	int called;
	const char *arg;
	int unset;
} length_cb;

static int length_callback(const struct option *opt, const char *arg, int unset)
{
	length_cb.called = 1;
	length_cb.arg = arg;
	length_cb.unset = unset;

	if (unset)
		return 1; /* do not support unset */

	*(int *)opt->value = strlen(arg);
	return 0;
}

static int number_callback(const struct option *opt, const char *arg, int unset)
{
	*(int *)opt->value = strtol(arg, NULL, 10);
	return 0;
}

int main(int argc, char **argv)
{
	const char *prefix = "prefix/";
	const char *usage[] = {
		"test-parse-options <options>",
		NULL
	};
	struct option options[] = {
		OPT_BOOL(0, "yes", &boolean, "get a boolean"),
		OPT_BOOL('D', "no-doubt", &boolean, "begins with 'no-'"),
		{ OPTION_SET_INT, 'B', "no-fear", &boolean, NULL,
		  "be brave", PARSE_OPT_NOARG | PARSE_OPT_NONEG, NULL, 1 },
		OPT_COUNTUP('b', "boolean", &boolean, "increment by one"),
		OPT_BIT('4', "or4", &boolean,
			"bitwise-or boolean with ...0100", 4),
		OPT_NEGBIT(0, "neg-or4", &boolean, "same as --no-or4", 4),
		OPT_GROUP(""),
		OPT_INTEGER('i', "integer", &integer, "get a integer"),
		OPT_INTEGER('j', NULL, &integer, "get a integer, too"),
		OPT_MAGNITUDE('m', "magnitude", &magnitude, "get a magnitude"),
		OPT_SET_INT(0, "set23", &integer, "set integer to 23", 23),
		OPT_DATE('t', NULL, &timestamp, "get timestamp of <time>"),
		OPT_CALLBACK('L', "length", &integer, "str",
			"get length of <str>", length_callback),
		OPT_FILENAME('F', "file", &file, "set file to <file>"),
		OPT_GROUP("String options"),
		OPT_STRING('s', "string", &string, "string", "get a string"),
		OPT_STRING(0, "string2", &string, "str", "get another string"),
		OPT_STRING(0, "st", &string, "st", "get another string (pervert ordering)"),
		OPT_STRING('o', NULL, &string, "str", "get another string"),
		OPT_NOOP_NOARG(0, "obsolete"),
		OPT_STRING_LIST(0, "list", &list, "str", "add str to list"),
		OPT_GROUP("Magic arguments"),
		OPT_ARGUMENT("quux", "means --quux"),
		OPT_NUMBER_CALLBACK(&integer, "set integer to NUM",
			number_callback),
		{ OPTION_COUNTUP, '+', NULL, &boolean, NULL, "same as -b",
		  PARSE_OPT_NOARG | PARSE_OPT_NONEG | PARSE_OPT_NODASH },
		{ OPTION_COUNTUP, 0, "ambiguous", &ambiguous, NULL,
		  "positive ambiguity", PARSE_OPT_NOARG | PARSE_OPT_NONEG },
		{ OPTION_COUNTUP, 0, "no-ambiguous", &ambiguous, NULL,
		  "negative ambiguity", PARSE_OPT_NOARG | PARSE_OPT_NONEG },
		OPT_GROUP("Standard options"),
		OPT__ABBREV(&abbrev),
		OPT__VERBOSE(&verbose, "be verbose"),
		OPT__DRY_RUN(&dry_run, "dry run"),
		OPT__QUIET(&quiet, "be quiet"),
		OPT_END(),
	};
	int i;
	struct strbuf output = STRBUF_INIT;

	argc = parse_options(argc, (const char **)argv, prefix, options, usage, 0);

	if (length_cb.called) {
		const char *arg = length_cb.arg;
		int unset = length_cb.unset;
		strbuf_addf(&output, "Callback: \"%s\", %d\n",
		       (arg ? arg : "not set"), unset);
	}
	strbuf_addf(&output, "boolean: %d\n", boolean);
	strbuf_addf(&output, "integer: %d\n", integer);
	strbuf_addf(&output, "magnitude: %lu\n", magnitude);
	strbuf_addf(&output, "timestamp: %lu\n", timestamp);
	strbuf_addf(&output, "string: %s\n", string ? string : "(not set)");
	strbuf_addf(&output, "abbrev: %d\n", abbrev);
	strbuf_addf(&output, "verbose: %d\n", verbose);
	strbuf_addf(&output, "quiet: %d\n", quiet);
	strbuf_addf(&output, "dry run: %s\n", dry_run ? "yes" : "no");
	strbuf_addf(&output, "file: %s\n", file ? file : "(not set)");

	for (i = 0; i < list.nr; i++)
		strbuf_addf(&output, "list: %s\n", list.items[i].string);

	for (i = 0; i < argc; i++)
		strbuf_addf(&output, "arg %02d: %s\n", i, argv[i]);

	printf("%s", output.buf);

	return 0;
}
