#include "cache.h"

static const char usage_str[] = "-v|--verbose (+|=|=+|=-|-)<seconds> <file>...";

int cmd_main(int ac, const char **av)
{
	int i;
	const char *command;

	if (ac < 2)
		die("too few arguments");

	command = av[1];

	if (!strcmp(command, "write")) {
		printf("writing args (ac: %d):\n", ac);
		for (i = 2; i < ac; i++) {
			printf("av[%d]: %s\n", i, av[i]);
		}
	} else {
		for (i = 0; i < ac; i++) {
			printf("av[%d]: %s\n", i, av[i]);
		}
	}

	return 0;
}
