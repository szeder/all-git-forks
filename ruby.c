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

static inline VALUE cstr_to_str(const char *str)
{
	if (str == NULL)
		return Qnil;
	return rb_str_new2(str);
}

static VALUE git_rb_setup_git_directory(VALUE self)
{
	int nongit_ok;
	const char *prefix;
	prefix = setup_git_directory_gently(&nongit_ok);
	return rb_ary_new3(2, cstr_to_str(prefix), INT2FIX(nongit_ok));
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

static VALUE git_rb_dwim_ref(VALUE self, VALUE name)
{
	unsigned char buf[20];
	char *ref;
	int r;

	r = dwim_ref(RSTRING_PTR(name), RSTRING_LEN(name), buf, &ref);
	return rb_ary_new3(3, sha1_to_str(buf), INT2NUM(r), cstr_to_str(ref));
}

static int git_config_fn(const char *var, const char *value, void *cb_data)
{
	VALUE r;
	r = rb_yield_values(2, rb_str_new2(var), rb_str_new2(value));
	return r == Qfalse;
}

static VALUE git_rb_git_config(VALUE self)
{
	int r;
	r = git_config(git_config_fn, NULL);
	return INT2FIX(r);
}

static VALUE git_rb_read_ref(VALUE self, VALUE refname)
{
	unsigned char sha1[20];
	if (read_ref(RSTRING_PTR(refname), sha1))
		return Qnil;
	return sha1_to_str(sha1);
}

static VALUE git_rb_peel_ref(VALUE self, VALUE refname)
{
	unsigned char sha1[20];
	if (peel_ref(RSTRING_PTR(refname), sha1))
		return Qnil;
	return sha1_to_str(sha1);
}

static VALUE git_rb_get_sha1(VALUE self, VALUE name)
{
	unsigned char buf[20];
	int r;
	r = get_sha1(RSTRING_PTR(name), buf);
	if (r)
		return Qnil;
	return sha1_to_str(buf);
}

static void git_ruby_init(void)
{
	rb_define_global_function("setup_git_directory", git_rb_setup_git_directory, 0);
	rb_define_global_function("for_each_ref", git_rb_for_each_ref, 0);
	rb_define_global_function("dwim_ref", git_rb_dwim_ref, 1);
	rb_define_global_function("git_config", git_rb_git_config, 0);
	rb_define_global_function("read_ref", git_rb_read_ref, 1);
	rb_define_global_function("peel_ref", git_rb_peel_ref, 1);
	rb_define_global_function("get_sha1", git_rb_get_sha1, 1);
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
