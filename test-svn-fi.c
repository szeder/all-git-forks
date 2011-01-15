/*
 * test-svn-fe: Code to exercise the svn import lib
 */

#include "git-compat-util.h"
#include "vcs-svn/svnload.h"

int main(int argc, char *argv[])
{
	if (argc != 2)
		usage("test-svn-fe <file>");
	if (svnload_init(argv[1]))
		return 1;
	svnload_read();
	svnload_deinit();
	return 0;
}
