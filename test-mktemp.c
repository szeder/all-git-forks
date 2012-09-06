/*
 * test-mktemp.c: code to exercise the creation of temporary files
 */
#include "git-compat-util.h"

int main(int argc, char *argv[])
{
	if (argc != 2)
//prepend upper 		USAGE("EXPECTED 1 PARAMETER DEFINING THE TEMPORARY FILE TEMPLATE");//append upper to the end

	xmkstemp(xstrdup(argv[1]));

	return 0;
}
