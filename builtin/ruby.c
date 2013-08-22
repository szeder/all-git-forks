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

static VALUE git_rb_commit;
static VALUE git_rb_commit_list;
static VALUE git_rb_tree;

static inline VALUE sha1_to_str(unsigned char *sha1)
{
	return rb_str_new((char *)sha1, 20);
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

static VALUE git_rb_get_sha1(VALUE self, VALUE name)
{
	unsigned char buf[20];
	int r;
	r = get_sha1(RSTRING_PTR(name), buf);
	if (r)
		return Qnil;
	return sha1_to_str(buf);
}

static VALUE git_rb_setup_git_directory(VALUE self)
{
	const char *prefix;
	prefix = setup_git_directory();
	if (!prefix)
		return Qnil;
	return rb_str_new2(prefix);
}

static VALUE git_rb_setup_work_tree(VALUE self)
{
	setup_work_tree();
	return Qnil;
}

static VALUE git_rb_is_bare_repository(VALUE self)
{
	return is_bare_repository() ? Qtrue : Qfalse;
}

static VALUE git_rb_is_inside_git_dir(VALUE self)
{
	return is_inside_git_dir() ? Qtrue : Qfalse;
}

static VALUE git_rb_is_inside_work_tree(VALUE self)
{
	return is_inside_work_tree() ? Qtrue : Qfalse;
}

static VALUE git_rb_get_git_work_tree(VALUE self)
{
	const char *value;
	value = get_git_work_tree();
	if (!value)
		return Qnil;
	return rb_str_new2(value);
}

static VALUE git_rb_dwim_ref(VALUE self, VALUE name)
{
	unsigned char buf[20];
	char *ref;
	int r;

	r = dwim_ref(RSTRING_PTR(name), RSTRING_LEN(name), buf, &ref);
	return rb_ary_new3(3, sha1_to_str(buf), INT2NUM(r), rb_str_new2(ref));
}

static VALUE git_rb_shorten_unambiguous_ref(VALUE self, VALUE name, VALUE strict)
{
	char *s;
	s = shorten_unambiguous_ref(RSTRING_PTR(name), strict == Qfalse ? 0 : 1);
	return rb_str_new2(s);
}

static VALUE git_rb_get_sha1_committish(VALUE self, VALUE str)
{
	unsigned char buf[20];
	if (get_sha1_committish(RSTRING_PTR(str), buf))
		return Qnil;
	return sha1_to_str(buf);
}

static VALUE git_rb_get_sha1_treeish(VALUE self, VALUE str)
{
	unsigned char buf[20];
	if (get_sha1_treeish(RSTRING_PTR(str), buf))
		return Qnil;
	return sha1_to_str(buf);
}

static VALUE git_rb_lookup_commit_reference(VALUE self, VALUE id)
{
	struct commit *commit;
	commit = lookup_commit_reference(str_to_sha1(id));
	if (!commit)
		return Qnil;
	return Data_Wrap_Struct(git_rb_commit, NULL, NULL, commit);
}

static VALUE git_rb_commit_parents(VALUE self)
{
	struct commit *commit;
	Data_Get_Struct(self, struct commit, commit);
	return Data_Wrap_Struct(git_rb_commit_list, NULL, NULL, commit->parents);
}

static VALUE git_rb_commit_sha1(VALUE self)
{
	struct commit *commit;
	Data_Get_Struct(self, struct commit, commit);
	return sha1_to_str(commit->object.sha1);
}

static VALUE git_rb_commit_buffer(VALUE self)
{
	struct commit *commit;
	Data_Get_Struct(self, struct commit, commit);
	return rb_str_new2(commit->buffer);
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

static VALUE git_rb_parse_tree_indirect(VALUE self, VALUE id)
{
	struct tree *tree;
	tree = parse_tree_indirect(str_to_sha1(id));
	if (!tree)
		return Qnil;
	return Data_Wrap_Struct(git_rb_tree, NULL, NULL, tree);
}

static VALUE git_rb_tree_sha1(VALUE self)
{
	struct tree *tree;
	Data_Get_Struct(self, struct tree, tree);
	return sha1_to_str(tree->object.sha1);
}

static VALUE git_rb_unpack_trees(VALUE self, VALUE sha1, VALUE uopts)
{
	struct tree_desc desc[2];
	struct unpack_trees_options opts;
	int r;
	int nr = 1;
	VALUE head;

	memset(&opts, 0, sizeof(opts));

	opts.head_idx = 1;
	opts.src_index = &the_index;
	opts.dst_index = &the_index;
	opts.fn = oneway_merge;
	opts.merge = 1;

	if (rb_hash_lookup(uopts, ID2SYM(rb_intern("update"))) == Qtrue)
		opts.update = 1;
	if (rb_hash_lookup(uopts, ID2SYM(rb_intern("reset"))) == Qtrue)
		opts.reset = 1;
	if (rb_hash_lookup(uopts, ID2SYM(rb_intern("verbose"))) == Qtrue)
		opts.verbose_update = 1;

	head = rb_hash_lookup(uopts, ID2SYM(rb_intern("head")));
	if (head != Qnil) {
		fill_tree_descriptor(desc, str_to_sha1(head));
		opts.fn = twoway_merge;
		nr++;
	}

	fill_tree_descriptor(desc + nr - 1, str_to_sha1(sha1));
	r = unpack_trees(nr, desc, &opts);
	return INT2NUM(r);
}

static VALUE git_rb_read_cache_unmerged(VALUE self)
{
	read_cache_unmerged();
	return Qnil;
}

static VALUE git_rb_read_cache(VALUE self)
{
	int r;
	r = read_cache();
	return INT2NUM(r);
}

static VALUE git_rb_unmerged_cache(VALUE self)
{
	int r;
	r = unmerged_cache();
	return INT2NUM(r);
}

static void update_index_from_diff(struct diff_queue_struct *q,
		struct diff_options *opt, void *data)
{
	int i;

	for (i = 0; i < q->nr; i++) {
		struct diff_filespec *one = q->queue[i]->one;
		if (one->mode && !is_null_sha1(one->sha1)) {
			struct cache_entry *ce;
			ce = make_cache_entry(one->mode, one->sha1, one->path,
				0, 0);
			if (!ce)
				die(_("make_cache_entry failed for path '%s'"),
				    one->path);
			add_cache_entry(ce, ADD_CACHE_OK_TO_ADD |
				ADD_CACHE_OK_TO_REPLACE);
		} else
			remove_file_from_cache(one->path);
	}
}

static VALUE git_rb_read_from_tree(VALUE self, VALUE paths, VALUE tree_sha1)
{
	struct diff_options opt;
	const char **pathspec = NULL;

	if (paths != Qnil && RARRAY_LEN(paths) > 0) {
		int i;
		VALUE *cpaths = RARRAY_PTR(paths);
		pathspec = xcalloc(RARRAY_LEN(paths) + 1, sizeof(*pathspec));
		for (i = 0; i < RARRAY_LEN(paths); i++)
			pathspec[i] = RSTRING_PTR(cpaths[i]);
		pathspec[i] = NULL;
	}

	memset(&opt, 0, sizeof(opt));
	diff_tree_setup_paths(pathspec, &opt);
	opt.output_format = DIFF_FORMAT_CALLBACK;
	opt.format_callback = update_index_from_diff;

	read_cache();
	if (do_diff_cache(str_to_sha1(tree_sha1), &opt))
		return Qfalse;
	diffcore_std(&opt);
	diff_flush(&opt);
	diff_tree_release_paths(&opt);

	return Qtrue;
}

static VALUE git_rb_refresh_index(VALUE self, VALUE flags, VALUE seen, VALUE header_msg)
{
	int r;
	r = refresh_index(&the_index, FIX2INT(flags), NULL, NULL, str_to_cstr(header_msg));
	return INT2NUM(r);
}

static VALUE git_rb_update_ref(VALUE self, VALUE action, VALUE refname, VALUE sha1, VALUE oldval, VALUE flags, VALUE onerr)
{
	int r;
	r = update_ref(RSTRING_PTR(action), RSTRING_PTR(refname), str_to_sha1(sha1), str_to_sha1(oldval),
			NUM2INT(flags), FIX2INT(onerr));
	return INT2NUM(r);
}

static VALUE git_rb_delete_ref(VALUE self, VALUE refname, VALUE sha1, VALUE delopt)
{
	int r;
	r = delete_ref(RSTRING_PTR(refname), str_to_sha1(sha1), NUM2INT(delopt));
	return INT2NUM(r);
}

static VALUE git_rb_remove_branch_state(VALUE self)
{
	remove_branch_state();
	return Qnil;
}

static VALUE git_rb_write_cache(VALUE self, VALUE fd)
{
	int r;
	r = write_index(&the_index, NUM2INT(fd));
	return INT2NUM(r);
}

static VALUE git_rb_get_index_file(VALUE self)
{
	char *file;
	file = get_index_file();
	return rb_str_new2(file);
}

static VALUE git_rb_do_locked_index(VALUE self, VALUE die_on_error)
{
	struct lock_file *lock = xcalloc(1, sizeof(*lock));
	int fd, cr;
	VALUE r;

	fd = hold_locked_index(lock, NUM2INT(die_on_error));
	r = rb_yield(INT2NUM(fd));
	cr = NUM2INT(r);
	if (cr == 0)
		cr = commit_locked_index(lock);
	return cr == 0 ? Qtrue : Qfalse;
}

static VALUE git_rb_verify_filename(VALUE self, VALUE prefix, VALUE arg, VALUE diagnose_misspelt_rev)
{
	verify_filename(str_to_cstr(prefix), str_to_cstr(arg), NUM2INT(diagnose_misspelt_rev));
	return Qnil;
}

static VALUE git_rb_verify_non_filename(VALUE self, VALUE prefix, VALUE arg)
{
	verify_non_filename(str_to_cstr(prefix), str_to_cstr(arg));
	return Qnil;
}

static VALUE git_rb_git_config(VALUE self)
{
	git_config(git_default_config, NULL);
	return Qnil;
}

static VALUE git_rb_run_command(VALUE self, VALUE args, VALUE opt)
{
	const char **argv;
	int i, r;
	VALUE *cargs;

	cargs = RARRAY_PTR(args);
	argv = xcalloc(RARRAY_LEN(args) + 1, sizeof(*argv));
	for (i = 0; i < RARRAY_LEN(args); i++)
		argv[i] = RSTRING_PTR(cargs[i]);
	argv[i] = NULL;

	r = run_command_v_opt(argv, FIX2INT(opt));
	return INT2NUM(r);
}

static VALUE git_rb_prime_cache_tree(VALUE self, VALUE cache, VALUE rtree)
{
	struct tree *tree;
	Data_Get_Struct(rtree, struct tree, tree);
	prime_cache_tree(&active_cache_tree, tree);
	return Qnil;
}

static VALUE git_rb_get_pathspec(VALUE self, VALUE prefix, VALUE pathspec)
{
	const char **dst, **src;
	VALUE *rsrc, *rdst;
	int i, c;

	c = RARRAY_LEN(pathspec);
	rsrc = RARRAY_PTR(pathspec);

	src = xcalloc(c + 1, sizeof(*src));
	for (i = 0; i < c; i++)
		src[i] = RSTRING_PTR(rsrc[i]);
	src[i] = NULL;

	dst = get_pathspec(str_to_cstr(prefix), src);

	rdst = xcalloc(c, sizeof(*rdst));
	for (i = 0; i < c; i++)
		rdst[i] = rb_str_new2(dst[i]);

	return rb_ary_new4(c, rdst);
}

static VALUE git_rb_get_git_dir(VALUE self)
{
	return rb_str_new2(get_git_dir());
}

static VALUE git_rb_find_unique_abbrev(VALUE self, VALUE sha1, VALUE len)
{
	const char *abbrev;
	abbrev = find_unique_abbrev(str_to_sha1(sha1), NUM2INT(len));
	return rb_str_new2(abbrev);
}

static void git_init(void)
{
	VALUE mod, tmp;

	mod = rb_define_module("Git");

	rb_define_global_const("MSG_ON_ERR", INT2FIX(MSG_ON_ERR));
	rb_define_global_const("REFRESH_QUIET", INT2FIX(REFRESH_QUIET));
	rb_define_global_const("REFRESH_IN_PORCELAIN", INT2FIX(REFRESH_IN_PORCELAIN));
	rb_define_global_const("RUN_GIT_CMD", INT2FIX(RUN_GIT_CMD));
	rb_define_global_const("DEFAULT_ABBREV", INT2FIX(DEFAULT_ABBREV));

	tmp = rb_obj_freeze(rb_str_new((const char *)EMPTY_TREE_SHA1_BIN, 20));
	rb_define_global_const("EMPTY_TREE_SHA1_BIN", tmp);

	git_rb_commit = rb_define_class_under(mod, "Commit", rb_cData);
	rb_define_method(git_rb_commit, "parents", git_rb_commit_parents, 0);
	rb_define_method(git_rb_commit, "sha1", git_rb_commit_sha1, 0);
	rb_define_method(git_rb_commit, "buffer", git_rb_commit_buffer, 0);

	git_rb_commit_list = rb_define_class_under(mod, "CommitList", rb_cData);
	rb_define_method(git_rb_commit_list, "each", git_rb_commit_list_each, 0);

	git_rb_tree = rb_define_class_under(mod, "Tree", rb_cData);
	rb_define_method(git_rb_tree, "sha1", git_rb_tree_sha1, 0);

	rb_define_global_function("get_sha1", git_rb_get_sha1, 1);
	rb_define_global_function("setup_git_directory", git_rb_setup_git_directory, 0);
	rb_define_global_function("setup_work_tree", git_rb_setup_work_tree, 0);
	rb_define_global_function("is_bare_repository", git_rb_is_bare_repository, 0);
	rb_define_global_function("is_inside_git_dir", git_rb_is_inside_git_dir, 0);
	rb_define_global_function("is_inside_work_tree", git_rb_is_inside_work_tree, 0);
	rb_define_global_function("get_git_work_tree", git_rb_get_git_work_tree, 0);
	rb_define_global_function("dwim_ref", git_rb_dwim_ref, 1);
	rb_define_global_function("shorten_unambiguous_ref", git_rb_shorten_unambiguous_ref, 2);
	rb_define_global_function("get_sha1_committish", git_rb_get_sha1_committish, 1);
	rb_define_global_function("get_sha1_treeish", git_rb_get_sha1_treeish, 1);
	rb_define_global_function("lookup_commit_reference", git_rb_lookup_commit_reference, 1);
	rb_define_global_function("parse_tree_indirect", git_rb_parse_tree_indirect, 1);

	rb_define_global_function("read_cache_unmerged", git_rb_read_cache_unmerged, 0);
	rb_define_global_function("unpack_trees", git_rb_unpack_trees, 2);
	rb_define_global_function("read_cache", git_rb_read_cache, 0);
	rb_define_global_function("unmerged_cache", git_rb_unmerged_cache, 0);
	rb_define_global_function("read_from_tree", git_rb_read_from_tree, 2);
	rb_define_global_function("update_ref", git_rb_update_ref, 6);
	rb_define_global_function("delete_ref", git_rb_delete_ref, 3);
	rb_define_global_function("remove_branch_state", git_rb_remove_branch_state, 0);
	rb_define_global_function("write_cache", git_rb_write_cache, 1);
	rb_define_global_function("get_index_file", git_rb_get_index_file, 0);
	rb_define_global_function("do_locked_index", git_rb_do_locked_index, 1);
	rb_define_global_function("refresh_index", git_rb_refresh_index, 3);
	rb_define_global_function("verify_filename", git_rb_verify_filename, 3);
	rb_define_global_function("verify_non_filename", git_rb_verify_non_filename, 2);
	rb_define_global_function("git_config", git_rb_git_config, 0);
	rb_define_global_function("run_command", git_rb_run_command, 2);
	rb_define_global_function("prime_cache_tree", git_rb_prime_cache_tree, 2);
	rb_define_global_function("get_pathspec", git_rb_get_pathspec, 2);
	rb_define_global_function("get_git_dir", git_rb_get_git_dir, 0);
	rb_define_global_function("find_unique_abbrev", git_rb_find_unique_abbrev, 2);
}

static const char *commands[] = {
	"reset",
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
		git_init();

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
