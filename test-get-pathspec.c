#include "cache.h"

int main(int argc, char **argv)
{
	int gitdir;
	const char *prefix = setup_git_directory_gently(&gitdir); /* get narrow_prefix */
	const char **p;

	/* make sure check_narrow_prefix() is called */
	get_git_dir();

	p = get_pathspec(prefix, (const char **)argv+1);
	if (!p)
		return 0;
	while (*p) {
		printf("%s\n", *p);
		p++;
	}
	return 0;
}
