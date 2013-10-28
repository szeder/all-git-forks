#include "cache.h"
#include "exec_cmd.h"
#include "refs.h"
#include "object.h"
#include "commit.h"
#include "remote.h"
#include "transport.h"
#include "revision.h"
#include "diff.h"
#include "shortlog.h"
#include "log-tree.h"

#undef NORETURN
#undef PATH_SEP

#include <ruby.h>

static VALUE git_rb_object;
static VALUE git_rb_commit;
static VALUE git_rb_commit_list;
static VALUE git_rb_remote;
static VALUE git_rb_transport;
static VALUE git_rb_ref;
static VALUE git_rb_rev_info;
static VALUE git_rb_diff_opt;

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

static VALUE git_rb_remote_get(VALUE self, VALUE name)
{
	struct remote *remote;
	remote = remote_get(RSTRING_PTR(name));
	if (!remote)
		return Qnil;
	return Data_Wrap_Struct(git_rb_remote, NULL, NULL, remote);
}

static VALUE git_rb_remote_url(VALUE self)
{
	struct remote *remote;
	VALUE url;
	int i;

	Data_Get_Struct(self, struct remote, remote);
	url = rb_ary_new2(remote->url_nr);
	for (i = 0; i < remote->url_nr; i++)
		rb_ary_store(url, i, rb_str_new2(remote->url[i]));
	return url;
}

static VALUE git_rb_transport_get(VALUE self, VALUE remote, VALUE url)
{
	struct transport *transport;
	struct remote *g_remote;
	Data_Get_Struct(remote, struct remote, g_remote);
	transport = transport_get(g_remote, str_to_cstr(url));
	if (!transport)
		return Qnil;
	return Data_Wrap_Struct(git_rb_transport, NULL, transport_disconnect, transport);
}

static VALUE git_rb_transport_get_remote_refs(VALUE self)
{
	struct transport *transport;
	const struct ref *ref;
	Data_Get_Struct(self, struct transport, transport);
	ref = transport_get_remote_refs(transport);
	return Data_Wrap_Struct(git_rb_ref, NULL, NULL, (void *)ref);
}

static VALUE git_rb_ref_each(VALUE self)
{
	struct ref *e, *ref;
	Data_Get_Struct(self, struct ref, ref);

	for (e = ref; e; e = e->next) {
		VALUE c;
		c = Data_Wrap_Struct(git_rb_ref, NULL, NULL, e);
		rb_yield(c);
	}

	return self;
}

static VALUE git_rb_ref_name(VALUE self)
{
	struct ref *ref;
	Data_Get_Struct(self, struct ref, ref);
	return rb_str_new2(ref->name);
}

static VALUE git_rb_ref_old_sha1(VALUE self)
{
	struct ref *ref;
	Data_Get_Struct(self, struct ref, ref);
	return sha1_to_str(ref->old_sha1);
}

static VALUE git_rb_find_unique_abbrev(VALUE self, VALUE sha1, VALUE len)
{
	const char *abbrev;
	abbrev = find_unique_abbrev(str_to_sha1(sha1), NUM2INT(len));
	return rb_str_new2(abbrev);
}

static VALUE git_rb_read_sha1_file(VALUE self, VALUE sha1, VALUE type)
{
	enum object_type g_type;
	void *buffer;
	unsigned long size;

	buffer = read_sha1_file(str_to_sha1(sha1), &g_type, &size);
	if (!buffer)
		return Qnil;
	return rb_ary_new3(2, rb_str_new(buffer, size), INT2FIX(g_type));
}

static VALUE git_rb_rev_info_alloc(VALUE class)
{
	struct rev_info *revs;
	return Data_Make_Struct(class, struct rev_info, NULL, free, revs);
}

static VALUE git_rb_rev_info_init(VALUE self, VALUE prefix)
{
	struct rev_info *revs;
	Data_Get_Struct(self, struct rev_info, revs);
	init_revisions(revs, str_to_cstr(prefix));
	return self;
}

static VALUE git_rb_rev_info_setup(VALUE self, VALUE args, VALUE opts)
{
	struct rev_info *revs;
	const char *argv[RARRAY_LEN(args) + 2];
	int i, r;

	argv[0] = "";
	Data_Get_Struct(self, struct rev_info, revs);
	for (i = 0; i < RARRAY_LEN(args); i++)
		argv[i + 1] = RSTRING_PTR(RARRAY_PTR(args)[i]);
	argv[i + 1] = NULL;
	r = setup_revisions(RARRAY_LEN(args) + 1, argv, revs, NULL);
	return INT2FIX(r - 1);
}

static VALUE git_rb_rev_info_single_setup(VALUE class, VALUE prefix, VALUE args, VALUE opts)
{
	struct rev_info *revs;
	VALUE self;
	self = Data_Make_Struct(class, struct rev_info, NULL, free, revs);
	init_revisions(revs, str_to_cstr(prefix));
	git_rb_rev_info_setup(self, args, opts);
	return self;
}

static VALUE git_rb_rev_info_each_revision(VALUE self, VALUE args, VALUE opts)
{
	struct commit *commit;
	struct rev_info *revs;

	Data_Get_Struct(self, struct rev_info, revs);
	if (prepare_revision_walk(revs))
		return Qnil;
	while ((commit = get_revision(revs))) {
		VALUE c;
		c = Data_Wrap_Struct(git_rb_commit, NULL, NULL, commit);
		rb_yield(c);
	}
	return Qnil;
}

static VALUE git_rb_rev_info_diffopt(VALUE self)
{
	struct rev_info *revs;

	Data_Get_Struct(self, struct rev_info, revs);
	return Data_Wrap_Struct(git_rb_diff_opt, NULL, NULL, &revs->diffopt);
}

static VALUE git_rb_shortlog(VALUE self, VALUE commits)
{
	struct shortlog log;
	int i;

	shortlog_init(&log);
	for (i = 0; i < RARRAY_LEN(commits); i++) {
		struct commit *commit;
		Data_Get_Struct(rb_ary_entry(commits, i), struct commit, commit);
		shortlog_add_commit(&log, commit);
	}
	shortlog_output(&log);
	return Qnil;
}

static VALUE git_rb_diff_tree_sha1(VALUE self, VALUE old, VALUE new, VALUE base, VALUE opt)
{
	struct diff_options *g_opt;
	int r;

	Data_Get_Struct(opt, struct diff_options, g_opt);

	r = diff_tree_sha1(str_to_sha1(old), str_to_sha1(new), str_to_cstr(base), g_opt);
	return INT2FIX(r);
}

static VALUE git_rb_log_tree_diff_flush(VALUE self, VALUE revs)
{
	struct rev_info *g_revs;
	int r;

	Data_Get_Struct(revs, struct rev_info, g_revs);
	r = log_tree_diff_flush(g_revs);
	return INT2FIX(r);
}

static VALUE git_rb_diff_opt_new(VALUE class)
{
	struct diff_options *opt;
	VALUE self;
	self = Data_Make_Struct(class, struct diff_options, NULL, free, opt);

	diff_setup(opt);

	return self;
}

static VALUE git_rb_diff_opt_method_missing(int argc, VALUE *argv, VALUE self)
{
	struct diff_options *opt;
	ID id;

	id = rb_to_id(argv[0]);
	Data_Get_Struct(self, struct diff_options, opt);

	if (id == rb_intern("stat_width="))
		opt->stat_width = NUM2INT(argv[1]);
	else if (id == rb_intern("stat_graph_width="))
		opt->stat_graph_width = NUM2INT(argv[1]);
	else if (id == rb_intern("output_format="))
		opt->output_format = NUM2INT(argv[1]);
	else if (id == rb_intern("detect_rename="))
		opt->detect_rename = NUM2INT(argv[1]);
	else if (id == rb_intern("flags="))
		opt->flags = NUM2INT(argv[1]);
	else if (id == rb_intern("output_format"))
		return INT2FIX(opt->output_format);
	else if (id == rb_intern("flags"))
		return INT2FIX(opt->flags);
	else
		return rb_call_super(argc, argv);
	return Qnil;
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

	rb_define_global_const("DEFAULT_ABBREV", INT2FIX(DEFAULT_ABBREV));

	rb_define_global_const("DIFF_FORMAT_RAW", INT2FIX(DIFF_FORMAT_RAW));
	rb_define_global_const("DIFF_FORMAT_DIFFSTAT", INT2FIX(DIFF_FORMAT_DIFFSTAT));
	rb_define_global_const("DIFF_FORMAT_NUMSTAT", INT2FIX(DIFF_FORMAT_NUMSTAT));
	rb_define_global_const("DIFF_FORMAT_SUMMARY", INT2FIX(DIFF_FORMAT_SUMMARY));
	rb_define_global_const("DIFF_FORMAT_PATCH", INT2FIX(DIFF_FORMAT_PATCH));
	rb_define_global_const("DIFF_FORMAT_SHORTSTAT", INT2FIX(DIFF_FORMAT_SHORTSTAT));
	rb_define_global_const("DIFF_FORMAT_DIRSTAT", INT2FIX(DIFF_FORMAT_DIRSTAT));
	rb_define_global_const("DIFF_FORMAT_NAME", INT2FIX(DIFF_FORMAT_NAME));
	rb_define_global_const("DIFF_FORMAT_NAME_STATUS", INT2FIX(DIFF_FORMAT_NAME_STATUS));
	rb_define_global_const("DIFF_FORMAT_CHECKDIFF", INT2FIX(DIFF_FORMAT_CHECKDIFF));
	rb_define_global_const("DIFF_FORMAT_NO_OUTPUT", INT2FIX(DIFF_FORMAT_NO_OUTPUT));
	rb_define_global_const("DIFF_FORMAT_CALLBACK", INT2FIX(DIFF_FORMAT_CALLBACK));

	rb_define_global_const("DIFF_DETECT_RENAME", INT2FIX(DIFF_DETECT_RENAME));
	rb_define_global_const("DIFF_DETECT_COPY", INT2FIX(DIFF_DETECT_COPY));

	rb_define_global_const("DIFF_OPT_RECURSIVE", INT2FIX(DIFF_OPT_RECURSIVE));
	rb_define_global_const("DIFF_OPT_TREE_IN_RECURSIVE", INT2FIX(DIFF_OPT_TREE_IN_RECURSIVE));
	rb_define_global_const("DIFF_OPT_BINARY", INT2FIX(DIFF_OPT_BINARY));
	rb_define_global_const("DIFF_OPT_TEXT", INT2FIX(DIFF_OPT_TEXT));
	rb_define_global_const("DIFF_OPT_FULL_INDEX", INT2FIX(DIFF_OPT_FULL_INDEX));
	rb_define_global_const("DIFF_OPT_SILENT_ON_REMOVE", INT2FIX(DIFF_OPT_SILENT_ON_REMOVE));
	rb_define_global_const("DIFF_OPT_FIND_COPIES_HARDER", INT2FIX(DIFF_OPT_FIND_COPIES_HARDER));
	rb_define_global_const("DIFF_OPT_FOLLOW_RENAMES", INT2FIX(DIFF_OPT_FOLLOW_RENAMES));
	rb_define_global_const("DIFF_OPT_RENAME_EMPTY", INT2FIX(DIFF_OPT_RENAME_EMPTY));
	rb_define_global_const("DIFF_OPT_HAS_CHANGES", INT2FIX(DIFF_OPT_HAS_CHANGES));
	rb_define_global_const("DIFF_OPT_QUICK", INT2FIX(DIFF_OPT_QUICK));
	rb_define_global_const("DIFF_OPT_NO_INDEX", INT2FIX(DIFF_OPT_NO_INDEX));
	rb_define_global_const("DIFF_OPT_ALLOW_EXTERNAL", INT2FIX(DIFF_OPT_ALLOW_EXTERNAL));
	rb_define_global_const("DIFF_OPT_EXIT_WITH_STATUS", INT2FIX(DIFF_OPT_EXIT_WITH_STATUS));
	rb_define_global_const("DIFF_OPT_REVERSE_DIFF", INT2FIX(DIFF_OPT_REVERSE_DIFF));
	rb_define_global_const("DIFF_OPT_CHECK_FAILED", INT2FIX(DIFF_OPT_CHECK_FAILED));
	rb_define_global_const("DIFF_OPT_RELATIVE_NAME", INT2FIX(DIFF_OPT_RELATIVE_NAME));
	rb_define_global_const("DIFF_OPT_IGNORE_SUBMODULES", INT2FIX(DIFF_OPT_IGNORE_SUBMODULES));
	rb_define_global_const("DIFF_OPT_DIRSTAT_CUMULATIVE", INT2FIX(DIFF_OPT_DIRSTAT_CUMULATIVE));
	rb_define_global_const("DIFF_OPT_DIRSTAT_BY_FILE", INT2FIX(DIFF_OPT_DIRSTAT_BY_FILE));
	rb_define_global_const("DIFF_OPT_ALLOW_TEXTCONV", INT2FIX(DIFF_OPT_ALLOW_TEXTCONV));
	rb_define_global_const("DIFF_OPT_DIFF_FROM_CONTENTS", INT2FIX(DIFF_OPT_DIFF_FROM_CONTENTS));
	rb_define_global_const("DIFF_OPT_SUBMODULE_LOG", INT2FIX(DIFF_OPT_SUBMODULE_LOG));
	rb_define_global_const("DIFF_OPT_DIRTY_SUBMODULES", INT2FIX(DIFF_OPT_DIRTY_SUBMODULES));
	rb_define_global_const("DIFF_OPT_IGNORE_UNTRACKED_IN_SUBMODULES", INT2FIX(DIFF_OPT_IGNORE_UNTRACKED_IN_SUBMODULES));
	rb_define_global_const("DIFF_OPT_IGNORE_DIRTY_SUBMODULES", INT2FIX(DIFF_OPT_IGNORE_DIRTY_SUBMODULES));
	rb_define_global_const("DIFF_OPT_OVERRIDE_SUBMODULE_CONFIG", INT2FIX(DIFF_OPT_OVERRIDE_SUBMODULE_CONFIG));
	rb_define_global_const("DIFF_OPT_DIRSTAT_BY_LINE", INT2FIX(DIFF_OPT_DIRSTAT_BY_LINE));
	rb_define_global_const("DIFF_OPT_FUNCCONTEXT", INT2FIX(DIFF_OPT_FUNCCONTEXT));
	rb_define_global_const("DIFF_OPT_PICKAXE_IGNORE_CASE", INT2FIX(DIFF_OPT_PICKAXE_IGNORE_CASE));

	rb_define_global_function("setup_git_directory", git_rb_setup_git_directory, 0);
	rb_define_global_function("for_each_ref", git_rb_for_each_ref, 0);
	rb_define_global_function("dwim_ref", git_rb_dwim_ref, 1);
	rb_define_global_function("git_config", git_rb_git_config, 0);
	rb_define_global_function("read_ref", git_rb_read_ref, 1);
	rb_define_global_function("peel_ref", git_rb_peel_ref, 1);
	rb_define_global_function("get_sha1", git_rb_get_sha1, 1);
	rb_define_global_function("get_merge_bases", git_rb_get_merge_bases, 2);
	rb_define_global_function("remote_get", git_rb_remote_get, 1);
	rb_define_global_function("transport_get", git_rb_transport_get, 2);
	rb_define_global_function("find_unique_abbrev", git_rb_find_unique_abbrev, 2);
	rb_define_global_function("read_sha1_file", git_rb_read_sha1_file, 1);
	rb_define_global_function("shortlog", git_rb_shortlog, 1);
	rb_define_global_function("diff_tree_sha1", git_rb_diff_tree_sha1, 4);
	rb_define_global_function("log_tree_diff_flush", git_rb_log_tree_diff_flush, 1);

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

	git_rb_remote = rb_define_class_under(mod, "Remote", rb_cData);
	rb_define_method(git_rb_remote, "url", git_rb_remote_url, 0);

	git_rb_transport = rb_define_class_under(mod, "Transport", rb_cData);
	rb_define_method(git_rb_transport, "get_remote_refs", git_rb_transport_get_remote_refs, 0);

	git_rb_ref = rb_define_class_under(mod, "Ref", rb_cData);
	rb_include_module(git_rb_ref, rb_mEnumerable);
	rb_define_method(git_rb_ref, "each", git_rb_ref_each, 0);
	rb_define_method(git_rb_ref, "name", git_rb_ref_name, 0);
	rb_define_method(git_rb_ref, "old_sha1", git_rb_ref_old_sha1, 0);

	git_rb_rev_info = rb_define_class_under(mod, "RevInfo", rb_cData);
	rb_include_module(git_rb_rev_info, rb_mEnumerable);
	rb_define_alloc_func(git_rb_rev_info, git_rb_rev_info_alloc);
	rb_define_method(git_rb_rev_info, "initialize", git_rb_rev_info_init, 1);
	rb_define_singleton_method(git_rb_rev_info, "setup", git_rb_rev_info_single_setup, 3);
	rb_define_method(git_rb_rev_info, "setup", git_rb_rev_info_setup, 2);
	rb_define_method(git_rb_rev_info, "each", git_rb_rev_info_each_revision, 0);
	rb_define_method(git_rb_rev_info, "diffopt", git_rb_rev_info_diffopt, 0);

	git_rb_diff_opt = rb_define_class_under(mod, "DiffOptions", rb_cData);
	rb_define_singleton_method(git_rb_diff_opt, "new", git_rb_diff_opt_new, 0);
	rb_define_method(git_rb_diff_opt, "method_missing", git_rb_diff_opt_method_missing, -1);
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
