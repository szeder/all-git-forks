#include "cache.h"
#include "exec_cmd.h"
#include "refs.h"
#include "object.h"
#include "commit.h"

#undef NORETURN
#undef PATH_SEP

#include <ruby.h>

static VALUE git_rb_object;
static VALUE git_rb_commit;
static VALUE git_rb_commit_list;

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

static inline char *str_to_cstr(VALUE str)
{
	if (str == Qnil)
		return NULL;
	return RSTRING_PTR(str);
}

static inline unsigned char *str_to_sha1(VALUE str)
{
	return (unsigned char *)str_to_cstr(str);
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

static VALUE git_rb_object_get(VALUE class, VALUE id)
{
	struct object *object;
	object = parse_object(str_to_sha1(id));
	return Data_Wrap_Struct(git_rb_object, NULL, NULL, object);
}

static VALUE git_rb_commit_get(VALUE class, VALUE id)
{
	struct object *object;
	object = parse_object(str_to_sha1(id));
	if (!object || object->type != OBJ_COMMIT)
		return Qnil;
	return Data_Wrap_Struct(git_rb_commit, NULL, NULL, object);
}

static VALUE git_rb_object_sha1(VALUE self)
{
	struct object *object;
	Data_Get_Struct(self, struct object, object);
	return sha1_to_str(object->sha1);
}

static VALUE git_rb_object_type(VALUE self)
{
	struct object *object;
	Data_Get_Struct(self, struct object, object);
	return INT2FIX(object->type);
}

static VALUE git_rb_object_to_s(VALUE self)
{
	struct object *object;
	Data_Get_Struct(self, struct object, object);
	return rb_str_new2(sha1_to_hex(object->sha1));
}

static VALUE git_rb_commit_buffer(VALUE self)
{
	struct commit *commit;
	Data_Get_Struct(self, struct commit, commit);
	return cstr_to_str(commit->buffer);
}

static VALUE git_rb_commit_list_each(VALUE self)
{
	struct commit_list *e, *list;
	Data_Get_Struct(self, struct commit_list, list);

	for (e = list; e; e = e->next) {
		VALUE c;
		c = Data_Wrap_Struct(git_rb_commit, NULL, NULL, e->item);
		rb_yield(c);
	}

	return self;
}

static VALUE git_rb_get_merge_bases(VALUE self, VALUE commits, VALUE cleanup)
{
	struct commit *g_commits[RARRAY_LEN(commits)];
	struct commit_list *result;
	int i;

	for (i = 0; i < RARRAY_LEN(commits); i++) {
		VALUE commit = RARRAY_PTR(commits)[i];
		Data_Get_Struct(commit, struct commit, g_commits[i]);
	}
	result = get_merge_bases_many(g_commits[0], RARRAY_LEN(commits) - 1, g_commits + 1, NUM2INT(cleanup));
	if (!result)
		return Qnil;
	return Data_Wrap_Struct(git_rb_commit_list, NULL, NULL, result);
}

static void git_ruby_init(void)
{
	VALUE mod;

	mod = rb_define_module("Git");

	rb_define_global_const("OBJ_BAD", INT2FIX(OBJ_BAD));
	rb_define_global_const("OBJ_NONE", INT2FIX(OBJ_NONE));
	rb_define_global_const("OBJ_COMMIT", INT2FIX(OBJ_COMMIT));
	rb_define_global_const("OBJ_TREE", INT2FIX(OBJ_TREE));
	rb_define_global_const("OBJ_BLOB", INT2FIX(OBJ_BLOB));
	rb_define_global_const("OBJ_TAG", INT2FIX(OBJ_TAG));
	rb_define_global_const("OBJ_OFS_DELTA", INT2FIX(OBJ_OFS_DELTA));
	rb_define_global_const("OBJ_REF_DELTA", INT2FIX(OBJ_REF_DELTA));
	rb_define_global_const("OBJ_ANY", INT2FIX(OBJ_ANY));
	rb_define_global_const("OBJ_MAX", INT2FIX(OBJ_MAX));

	rb_define_global_function("setup_git_directory", git_rb_setup_git_directory, 0);
	rb_define_global_function("for_each_ref", git_rb_for_each_ref, 0);
	rb_define_global_function("dwim_ref", git_rb_dwim_ref, 1);
	rb_define_global_function("git_config", git_rb_git_config, 0);
	rb_define_global_function("read_ref", git_rb_read_ref, 1);
	rb_define_global_function("peel_ref", git_rb_peel_ref, 1);
	rb_define_global_function("get_sha1", git_rb_get_sha1, 1);
	rb_define_global_function("get_merge_bases", git_rb_get_merge_bases, 2);

	git_rb_object = rb_define_class_under(mod, "Object", rb_cData);
	rb_define_singleton_method(git_rb_object, "get", git_rb_object_get, 1);
	rb_define_method(git_rb_object, "sha1", git_rb_object_sha1, 0);
	rb_define_method(git_rb_object, "type", git_rb_object_type, 0);
	rb_define_method(git_rb_object, "to_s", git_rb_object_to_s, 0);

	git_rb_commit = rb_define_class_under(mod, "Commit", git_rb_object);
	rb_define_singleton_method(git_rb_commit, "get", git_rb_commit_get, 1);
	rb_define_method(git_rb_commit, "buffer", git_rb_commit_buffer, 0);

	git_rb_commit_list = rb_define_class_under(mod, "CommitList", rb_cData);
	rb_include_module(git_rb_commit_list, rb_mEnumerable);
	rb_define_method(git_rb_commit_list, "each", git_rb_commit_list_each, 0);
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
