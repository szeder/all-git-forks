#include "git-compat-util.h"
#include "exec_cmd.h"

int main(int argc, const char **argv)
{
	argv[0] = git_extract_argv0_path(argv[0]);

	return cmd_main(argc, argv);
}
