#include "cache.h"
#include "string-list.h"

int main(int argc, char **argv)
{
	if ((argc == 4 || argc == 5) && !strcmp(argv[1], "split_in_place")) {
		struct string_list list = STRING_LIST_INIT_NODUP;
		int i;
		char *s = xstrdup(argv[2]);
		int delim = *argv[3];
		int maxsplit = (argc == 5) ? atoi(argv[4]) : -1;

		i = string_list_split_in_place(&list, s, delim, maxsplit);
		printf("%d\n", i);
		for (i = 0; i < list.nr; i++)
			printf("[%d]: \"%s\"\n", i, list.items[i].string);
		string_list_clear(&list, 0);
		free(s);
		return 0;
	}

	fprintf(stderr, "%s: unknown function name: %s\n", argv[0],
		argv[1] ? argv[1] : "(there was none)");
	return 1;
}
