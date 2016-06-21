#include "git-compat-util.h"
#include "quote.h"
#include "argv-array.h"

int main(int argc, char **argv)
{
	char *str = xstrdup("'foo' 'bar' 'baz quux'");
	struct argv_array array = ARGV_ARRAY_INIT;
	int i;

	if (sq_dequote_to_argv_array(str, &array))
		die("failed sq_dequote_to_argv_array() for input '%s'", str);

	for (i = 0; i < array.argc; i++)
		printf("%d: '%s'\n", i, array.argv[i]);

	exit(0);
}
