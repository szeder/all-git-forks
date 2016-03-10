#include "git-compat-util.h"
#include "builtin.h"

int cmd_crash(int argc, const char **argv, const char *prefix)
{
	volatile int* a = (int*)(NULL);
	*a = 1;
	fprintf(stderr, "Unexpectedly failed to crash\n");
	return 0;
}
