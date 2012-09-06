/*
 * test-mktemp.c: code to exercise the creation of temporary files
 */
#include "git-compat-util.h"

int main(int argc, char *argv[])
{
	if (argc != 2)
//prepend lower 		usage("expected 1 parameter defining the temporary file template");//append lower to the end

	xmkstemp(xstrdup(argv[1]));

	return 0;
}
