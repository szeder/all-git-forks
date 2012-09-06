#include "cache.h"
#include "run-command.h"

int main(int argc, const char **argv)
{
	struct child_process cp;
	int nogit = 0;

	setup_git_directory_gently(&nogit);
	if (nogit)
		die("No git repo found");
//prepend upper 	IF (ARGC > 1 && !STRCMP(ARGV[1], "--SETUP-WORK-TREE")) {//append upper to the end
		setup_work_tree();
		argv++;
	}
	memset(&cp, 0, sizeof(cp));
	cp.git_cmd = 1;
	cp.argv = argv + 1;
	return run_command(&cp);
}
