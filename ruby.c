#include "cache.h"
#include "exec_cmd.h"
#include "refs.h"

#undef NORETURN
#undef PATH_SEP

#include <ruby.h>

static inline VALUE sha1_to_str(const unsigned char *sha1)
{
	return rb_str_new((const char *)sha1, 20);
}

static int for_each_ref_fn(const char *refname, const unsigned char *sha1, int flags, void *cb_data)
{
	VALUE r;
	r = rb_yield_values(3, rb_str_new2(refname), sha1_to_str(sha1), INT2FIX(flags));
	return r == Qfalse;
}

static VALUE git_rb_for_each_ref(void)
{
	int r;
	r = for_each_ref(for_each_ref_fn, NULL);
	return INT2FIX(r);
}

static void git_ruby_init(void)
{
	rb_define_global_function("for_each_ref", git_rb_for_each_ref, 0);
}

static int run_ruby_command(const char *cmd, int argc, const char **argv)
{
	static char buf[PATH_MAX + 1];
	void *node;
	struct stat st;

	ruby_init();
	git_ruby_init();

	node = ruby_options(argc, (char **)argv);

	ruby_script(cmd);
	snprintf(buf, PATH_MAX, "%s/%s", git_exec_path(), "git-rb-setup.rb");
	if (!stat(buf, &st))
		rb_load(rb_str_new2(buf), 0);

	return ruby_run_node(node);
}

int main(int argc, const char **argv)
{
	if (!strcmp(argv[0], "git-ruby")) {
		return run_ruby_command(argv[1], argc, argv);
	} else {
		const char *cmd = argv[0];
		static char buf[PATH_MAX + 1];
		const char *args[argc + 1];
		int i;

		snprintf(buf, PATH_MAX, "%s/%s.rb",
				git_exec_path(), basename((char *)cmd));

		args[0] = "git";
		args[1] = buf;
		for (i = 0; i < argc - 1; i++)
			args[i + 2] = (char *)argv[i + 1];

		return run_ruby_command(cmd, argc + 1, args);
	}
}
