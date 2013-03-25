#include "cache.h"

static int config_strbuf(const char *var, const char *value, void *data)
{
	printf("var: %s, value: %s\n", var, value);

	return 1;
}

static void die_usage(int argc, char **argv)
{
	fprintf(stderr, "Usage: %s strbuf <name>\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	if (argc < 3)
		die_usage(argc, argv);

	if (!strcmp(argv[1], "strbuf")) {
		struct strbuf buf = STRBUF_INIT;
		const char *name = argv[2];

		while (strbuf_fread(&buf, 1024, stdin) == 1024)
			;

		if (ferror(stdin))
			die("An error occurred while reading from stdin");

		git_config_from_strbuf(config_strbuf, name, &buf, NULL);
		strbuf_release(&buf);

		return 0;
	}

	die_usage(argc, argv);

	return 1;
}
