#include "cache.h"

static const char usage_str[] = "(write|delete|read) <args>...";

static void write_s_i_info(int ac, const char **av)
{
	int i;

	printf("writing args (ac: %d):\n", ac);
	for (i = 2; i < ac; i++) {
		printf("av[%d]: %s\n", i, av[i]);
	}
}

static void show_args(int ac, const char **av)
{
	int i;

	for (i = 0; i < ac; i++) {
		printf("av[%d]: %s\n", i, av[i]);
	}
}

int cmd_main(int ac, const char **av)
{
	const char *command;

	if (ac < 2)
		die("%s\nusage: %s %s", "too few arguments", av[0], usage_str);

	command = av[1];

	if (!strcmp(command, "write"))
		write_s_i_info(ac, av);
	else
		show_args(ac, av);

	return 0;
}
