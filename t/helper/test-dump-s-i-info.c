#include "cache.h"

static const char usage_str[] = "(write|delete|read) <args>...";
static const char write_usage_str[] = "write <shared-index> <path>";

static void write_s_i_info(int ac, const char **av)
{
	const char *shared_index;
	const char *path;

	if (ac != 4)
		die("%s\nusage: %s %s",
		    "write command requires exactly 2 arguments",
		    av[0], write_usage_str);

	shared_index = av[2];
	path = av[3];

	printf("writing shared_index: %s\n", shared_index);
	printf("writing path: %s\n", path);
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
