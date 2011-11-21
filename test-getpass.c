#include "cache.h"
#include "git-compat-util.h"

int main(int argc, char *argv[])
{
	printf("%s\n", git_getpass("gimme your password: "));
	return 0;
}
