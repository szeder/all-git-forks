#include "cache.h"
#include "exec_cmd.h"

int main(int argc, const char **argv)
{
	/*
	 * Always open file descriptors 0/1/2 to avoid clobbering files
	 * in die().  It also avoids messing up when the pipes are dup'ed
	 * onto stdin/stdout/stderr in the child processes we spawn.
	 */
	sanitize_stdfds();

	argv[0] = git_extract_argv0_path(argv[0]);

	return cmd_main(argc, argv);
}
