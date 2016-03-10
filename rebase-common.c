#include "cache.h"
#include "rebase-common.h"
#include "lockfile.h"
#include "revision.h"
#include "refs.h"
#include "unpack-trees.h"
#include "branch.h"

void refresh_and_write_cache(unsigned int flags)
{
	struct lock_file *lock_file = xcalloc(1, sizeof(struct lock_file));

	hold_locked_index(lock_file, 1);
	refresh_cache(flags);
	if (write_locked_index(&the_index, lock_file, COMMIT_LOCK))
		die(_("unable to write index file"));
}

int cache_has_unstaged_changes(void)
{
	struct rev_info rev_info;
	int result;

	init_revisions(&rev_info, NULL);
	DIFF_OPT_SET(&rev_info.diffopt, IGNORE_SUBMODULES);
	DIFF_OPT_SET(&rev_info.diffopt, QUICK);
	diff_setup_done(&rev_info.diffopt);
	result = run_diff_files(&rev_info, 0);
	return diff_result_code(&rev_info.diffopt, result);
}

int cache_has_uncommitted_changes(void)
{
	struct rev_info rev_info;
	int result;

	if (is_cache_unborn())
		return 0;

	init_revisions(&rev_info, NULL);
	DIFF_OPT_SET(&rev_info.diffopt, IGNORE_SUBMODULES);
	DIFF_OPT_SET(&rev_info.diffopt, QUICK);
	add_head_to_pending(&rev_info);
	diff_setup_done(&rev_info.diffopt);
	result = run_diff_index(&rev_info, 1);
	return diff_result_code(&rev_info.diffopt, result);
}

void rebase_die_on_unclean_worktree(void)
{
	int do_die = 0;

	refresh_and_write_cache(REFRESH_QUIET);

	if (cache_has_unstaged_changes()) {
		error(_("Cannot rebase: You have unstaged changes."));
		do_die = 1;
	}

	if (cache_has_uncommitted_changes()) {
		if (do_die)
			error(_("Additionally, your index contains uncommitted changes."));
		else
			error(_("Cannot rebase: Your index contains uncommitted changes."));
		do_die = 1;
	}

	if (do_die)
		exit(1);
}

static void reset_refs(const struct object_id *oid)
{
	struct object_id *orig, oid_orig;
	struct object_id *old_orig, oid_old_orig;

	if (!get_oid("ORIG_HEAD", &oid_old_orig))
		old_orig = &oid_old_orig;
	if (!get_oid("HEAD", &oid_orig)) {
		orig = &oid_orig;
		update_ref("updating ORIG_HEAD", "ORIG_HEAD",
				orig ? orig->hash : NULL,
				old_orig ? old_orig->hash : NULL,
				0, UPDATE_REFS_MSG_ON_ERR);
	} else if (old_orig)
		delete_ref("ORIG_HEAD", old_orig->hash, 0);
	update_ref("updating HEAD", "HEAD", oid->hash, orig ? orig->hash : NULL, 0, UPDATE_REFS_MSG_ON_ERR);
}

int reset_hard(const struct object_id *commit)
{
	struct tree *tree;
	struct tree_desc desc[1];
	struct unpack_trees_options opts;
	struct lock_file *lock_file;

	tree = parse_tree_indirect(commit->hash);
	if (!tree)
		return error(_("Could not parse object '%s'."), oid_to_hex(commit));

	lock_file = xcalloc(1, sizeof(*lock_file));
	hold_locked_index(lock_file, 1);

	if (refresh_cache(REFRESH_QUIET) < 0) {
		rollback_lock_file(lock_file);
		return -1;
	}

	memset(&opts, 0, sizeof(opts));
	opts.head_idx = 1;
	opts.src_index = &the_index;
	opts.dst_index = &the_index;
	opts.fn = oneway_merge;
	opts.merge = 1;
	opts.update = 1;
	opts.reset = 1;
	init_tree_desc(&desc[0], tree->buffer, tree->size);

	if (unpack_trees(1, desc, &opts) < 0) {
		rollback_lock_file(lock_file);
		return -1;
	}

	if (write_locked_index(&the_index, lock_file, COMMIT_LOCK) < 0)
		die(_("unable to write new index file"));

	reset_refs(commit);
	remove_branch_state();

	return 0;
}

void rebase_options_init(struct rebase_options *opts)
{
	oidclr(&opts->onto);
	opts->onto_name = NULL;

	oidclr(&opts->upstream);

	oidclr(&opts->orig_head);
	opts->orig_refname = NULL;

	opts->resolvemsg = NULL;
}

void rebase_options_release(struct rebase_options *opts)
{
	free(opts->onto_name);
	free(opts->orig_refname);
}

void rebase_options_swap(struct rebase_options *dst, struct rebase_options *src)
{
	struct rebase_options tmp = *dst;
	*dst = *src;
	*src = tmp;
}

static int state_file_exists(const char *dir, const char *file)
{
	return file_exists(mkpath("%s/%s", dir, file));
}

static int read_state_file(struct strbuf *sb, const char *dir, const char *file)
{
	const char *path = mkpath("%s/%s", dir, file);
	strbuf_reset(sb);
	if (strbuf_read_file(sb, path, 0) >= 0)
		return sb->len;
	else
		return error(_("could not read '%s'"), path);
}

int rebase_options_load(struct rebase_options *opts, const char *dir)
{
	struct strbuf sb = STRBUF_INIT;
	const char *filename;

	/* opts->orig_refname */
	if (read_state_file(&sb, dir, "head-name") < 0)
		return -1;
	strbuf_trim(&sb);
	if (starts_with(sb.buf, "refs/heads/"))
		opts->orig_refname = strbuf_detach(&sb, NULL);
	else if (!strcmp(sb.buf, "detached HEAD"))
		opts->orig_refname = NULL;
	else
		return error(_("could not parse %s"), mkpath("%s/%s", dir, "head-name"));

	/* opts->onto */
	if (read_state_file(&sb, dir, "onto") < 0)
		return -1;
	strbuf_trim(&sb);
	if (get_oid_hex(sb.buf, &opts->onto) < 0)
		return error(_("could not parse %s"), mkpath("%s/%s", dir, "onto"));

	/*
	 * We always write to orig-head, but interactive rebase used to write
	 * to head. Fall back to reading from head to cover for the case that
	 * the user upgraded git with an ongoing interactive rebase.
	 */
	filename = state_file_exists(dir, "orig-head") ? "orig-head" : "head";
	if (read_state_file(&sb, dir, filename) < 0)
		return -1;
	strbuf_trim(&sb);
	if (get_oid_hex(sb.buf, &opts->orig_head) < 0)
		return error(_("could not parse %s"), mkpath("%s/%s", dir, filename));

	strbuf_release(&sb);
	return 0;
}

static int write_state_text(const char *dir, const char *file, const char *string)
{
	return write_file(mkpath("%s/%s", dir, file), "%s", string);
}

void rebase_options_save(const struct rebase_options *opts, const char *dir)
{
	const char *head_name = opts->orig_refname;
	if (!head_name)
		head_name = "detached HEAD";
	write_state_text(dir, "head-name", head_name);
	write_state_text(dir, "onto", oid_to_hex(&opts->onto));
	write_state_text(dir, "orig-head", oid_to_hex(&opts->orig_head));
}
