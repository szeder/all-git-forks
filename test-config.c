#include "cache.h"

static const char *config_string = "[some]\n"
			    "	value = content\n";

static int config_strbuf(const char *var, const char *value, void *data)
{
	int *success = data;
	if (!strcmp(var, "some.value") && !strcmp(value, "content"))
		*success = 0;

	printf("var: %s, value: %s\n", var, value);

	return 1;
}

static void die_usage(int argc, char **argv)
{
	fprintf(stderr, "Usage: %s strbuf\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	if (argc < 2)
		die_usage(argc, argv);

	if (!strcmp(argv[1], "strbuf")) {
		int success = 1;
		struct strbuf buf = STRBUF_INIT;

		strbuf_addstr(&buf, config_string);
		git_config_from_strbuf(config_strbuf, &buf, &success);

		return success;
	}

	die_usage(argc, argv);

	return 1;
}
