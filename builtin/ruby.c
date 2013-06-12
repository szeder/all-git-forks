#include "cache.h"
#include "exec_cmd.h"
#include "refs.h"
#include "commit.h"
#include "tree-walk.h"
#include "unpack-trees.h"
#include "diff.h"
#include "diffcore.h"
#include "branch.h"
#include "run-command.h"
#include "cache-tree.h"

#undef NORETURN
#undef PATH_SEP

#include <ruby.h>

static const char *commands[] = {
};

void handle_ruby_command(int argc, const char **argv)
{
	const char *cmd = argv[0];
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		static char buf[PATH_MAX + 1];
		const char *dir;
		char *args[argc + 2];
		void *node;
		VALUE prefix;

		if (strcmp(commands[i], cmd))
			continue;

		dir = git_exec_path();
		snprintf(buf, PATH_MAX, "%s/git-%s.rb", dir, cmd);

		ruby_init();

		prefix = Qnil;
		rb_define_variable("$prefix", &prefix);

		args[0] = "git";
		args[1] = buf;
		for (i = 0; i < argc; i++)
			args[i + 2] = (char*)argv[i];
		node = ruby_options(argc + 2, args);

		exit(ruby_run_node(node));
	}
}
