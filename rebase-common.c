#include "cache.h"
#include "rebase-common.h"
#include "strbuf.h"
#include "dir.h"
#include "quote.h"
#include "run-command.h"
#include "refs.h"
#include "commit.h"
#include "branch.h"
#include "diff.h"
#include "lockfile.h"
#include "revision.h"
#include "remote.h"
#include "unpack-trees.h"
#include "notes-utils.h"

int refresh_and_write_cache(unsigned int flags)
{
	struct lock_file *lock_file = xcalloc(1, sizeof(struct lock_file));

	hold_locked_index(lock_file, 1);
	if (refresh_cache(flags)) {
		rollback_lock_file(lock_file);
		return -1;
	}
	if (write_locked_index(&the_index, lock_file, COMMIT_LOCK))
		return error(_("unable to write index file"));
	return 0;
}

int cache_has_unstaged_changes(int ignore_submodules)
{
	struct rev_info rev_info;
	int result;

	init_revisions(&rev_info, NULL);
	if (ignore_submodules)
		DIFF_OPT_SET(&rev_info.diffopt, IGNORE_SUBMODULES);
	DIFF_OPT_SET(&rev_info.diffopt, QUICK);
	diff_setup_done(&rev_info.diffopt);
	result = run_diff_files(&rev_info, 0);
	return diff_result_code(&rev_info.diffopt, result);
}

int cache_has_uncommitted_changes(int ignore_submodules)
{
	struct rev_info rev_info;
	int result;

	if (is_cache_unborn())
		return 0;

	init_revisions(&rev_info, NULL);
	if (ignore_submodules)
		DIFF_OPT_SET(&rev_info.diffopt, IGNORE_SUBMODULES);
	DIFF_OPT_SET(&rev_info.diffopt, QUICK);
	add_head_to_pending(&rev_info);
	diff_setup_done(&rev_info.diffopt);
	result = run_diff_index(&rev_info, 1);
	return diff_result_code(&rev_info.diffopt, result);
}

void rebase_die_on_unclean_worktree(int ignore_submodules)
{
	int do_die = 0;

	if (refresh_and_write_cache(REFRESH_QUIET) < 0)
		die(_("failed to refresh index"));

	if (cache_has_unstaged_changes(ignore_submodules)) {
		error(_("Cannot rebase: You have unstaged changes."));
		do_die = 1;
	}

	if (cache_has_uncommitted_changes(ignore_submodules)) {
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

int copy_notes_for_rebase(const char *rewritten_file)
{
	struct notes_rewrite_cfg *c;
	struct strbuf sb = STRBUF_INIT;
	const char *invalid_line = _("Malformed input line: '%s'.");
	const char *msg = "Notes added by 'git rebase'";
	FILE *fp;
	int ret = 0;

	c = init_copy_notes_for_rewrite("rebase");
	if (!c)
		return 0;

	fp = fopen(rewritten_file, "r");
	if (!fp)
		return -1;

	while (!strbuf_getline_lf(&sb, fp)) {
		struct object_id from_obj, to_obj;

		if (get_oid_hex(sb.buf, &from_obj)) {
			ret = error(invalid_line, sb.buf);
			goto finish;
		}

		if (sb.buf[GIT_SHA1_HEXSZ] != ' ') {
			ret = error(invalid_line, sb.buf);
			goto finish;
		}

		if (get_oid_hex(sb.buf + GIT_SHA1_HEXSZ + 1, &to_obj)) {
			ret = error(invalid_line, sb.buf);
			goto finish;
		}

		if (copy_note_for_rewrite(c, from_obj.hash, to_obj.hash))
			ret = error(_("Failed to copy notes from '%s' to '%s'"),
					oid_to_hex(&from_obj), oid_to_hex(&to_obj));
	}

finish:
	finish_copy_notes_for_rewrite(c, msg);
	fclose(fp);
	strbuf_release(&sb);
	return ret;
}

void rebase_options_init(struct rebase_options *opts)
{
	oidclr(&opts->onto);
	opts->onto_name = NULL;

	oidclr(&opts->upstream);

	oidclr(&opts->orig_head);
	opts->orig_refname = NULL;

	opts->quiet = 0;
	opts->verbose = 0;
	opts->strategy = NULL;
	argv_array_init(&opts->strategy_opts);
	opts->allow_rerere_autoupdate = NULL;
	opts->gpg_sign_opt = NULL;
	opts->resolvemsg = NULL;
	opts->force = 0;
	opts->root = 0;

	opts->autostash = 0;
}

void rebase_options_release(struct rebase_options *opts)
{
	free(opts->onto_name);
	free(opts->orig_refname);
	free(opts->strategy);
	argv_array_clear(&opts->strategy_opts);
	free(opts->gpg_sign_opt);
}

void rebase_options_swap(struct rebase_options *dst, struct rebase_options *src)
{
	struct rebase_options tmp = *dst;
	*dst = *src;
	*src = tmp;
}

static const char *refname_to_branchname(const char *refname)
{
	const char *branchname;
	if (!refname)
		return "HEAD";
	else if (skip_prefix(refname, "refs/heads/", &branchname))
		return branchname;
	else
		return refname;
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

/**
 * For convenience to call write_file()
 */
static int write_state_text(const char *dir, const char *file, const char *string)
{
	return write_file(mkpath("%s/%s", dir, file), "%s", string);
}

/**
 * read_basic_state() in git-rebase.sh
 *
 * DONE
 */
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

	/* opts->quiet */
	if (read_state_file(&sb, dir, "quiet") < 0)
		return -1;
	strbuf_trim(&sb);
	opts->quiet = !strcmp(sb.buf, "t");

	/* opts->verbose */
	if (state_file_exists(dir, "verbose"))
		opts->verbose = 1;

	/* opts->strategy */
	if (state_file_exists(dir, "strategy")) {
		if (read_state_file(&sb, dir, "strategy") < 0)
			return -1;
		assert(!opts->strategy);
		opts->strategy = strbuf_detach(&sb, NULL);
	}

	/* opts->strategy_opts */
	if (state_file_exists(dir, "strategy_opts")) {
		if (read_state_file(&sb, dir, "strategy_opts") < 0)
			return -1;
		if (sq_dequote_to_argv_array(sb.buf, &opts->strategy_opts) < 0)
			die(_("could not parse %s"), mkpath("%s/strategy_opts", dir));
	}

	/* opts->allow_rerere_autoupdate */
	if (state_file_exists(dir, "allow_rerere_autoupdate")) {
		if (read_state_file(&sb, dir, "allow_rerere_autoupdate") < 0)
			return -1;
		assert(!opts->allow_rerere_autoupdate);
		opts->allow_rerere_autoupdate = strbuf_detach(&sb, NULL);
	}

	/* opts->gpg_sign_opt */
	if (state_file_exists(dir, "gpg_sign_opt")) {
		if (read_state_file(&sb, dir, "gpg_sign_opt") < 0)
			return -1;
		assert(!opts->gpg_sign_opt);
		opts->gpg_sign_opt = strbuf_detach(&sb, NULL);
	}

	strbuf_release(&sb);
	return 0;
}

/**
 * write_basic_state() in git-rebase.sh
 *
 * DONE
 */
void rebase_options_save(const struct rebase_options *opts, const char *dir)
{
	struct strbuf sb = STRBUF_INIT;

	write_state_text(dir, "head-name", opts->orig_refname ? opts->orig_refname : "detached HEAD");
	write_state_text(dir, "onto", oid_to_hex(&opts->onto));
	write_state_text(dir, "orig-head", oid_to_hex(&opts->orig_head));
	write_state_text(dir, "quiet", opts->quiet ? "t" : "f");
	if (opts->verbose)
		write_state_text(dir, "verbose", "t");
	if (opts->strategy)
		write_state_text(dir, "strategy", opts->strategy);
	if (opts->strategy_opts.argc) {
		sq_quote_argv(&sb, opts->strategy_opts.argv, 0);
		write_state_text(dir, "strategy_opts", sb.buf);
	}
	if (opts->allow_rerere_autoupdate)
		write_state_text(dir, "allow_rerere_autoupdate", opts->allow_rerere_autoupdate);
	if (opts->gpg_sign_opt)
		write_state_text(dir, "gpg_sign_opt", opts->gpg_sign_opt);

	strbuf_release(&sb);
}

static int is_linear_history(struct commit *from, struct commit *to)
{
	struct commit *commit = to;

	if (!in_merge_bases(from, to))
		return 0;

	while (commit != from) {
		if (commit->parents->next)
			return 0;
		commit = commit->parents->item;
	}

	return 1;
}

static int detach_head(const struct object_id *commit, const char *onto_name)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	int status;
	const char *reflog_action = getenv("GIT_REFLOG_ACTION");
	if (!reflog_action)
		reflog_action = "rebase";
	cp.git_cmd = 1;
	argv_array_pushf(&cp.env_array, "GIT_REFLOG_ACTION=%s: checkout %s",
			reflog_action, onto_name ? onto_name : oid_to_hex(commit));
	argv_array_push(&cp.args, "checkout");
	argv_array_push(&cp.args, "-q");
	argv_array_push(&cp.args, "--detach");
	argv_array_push(&cp.args, oid_to_hex(commit));
	status = run_command(&cp);

	/* reload cache as checkout will have modified it */
	discard_cache();
	read_cache();

	return status;
}

static int checkout_refname(const char *refname)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	struct branch *curr_branch;
	const char *reflog_action;
	int status;

	curr_branch = branch_get("HEAD");
	if (curr_branch && !strcmp(refname, curr_branch->refname))
		return 0; /* already on the branch */

	reflog_action = getenv("GIT_REFLOG_ACTION");
	if (!reflog_action)
		reflog_action = "rebase";

	cp.git_cmd = 1;
	argv_array_pushf(&cp.env_array, "GIT_REFLOG_ACTION=%s: checkout %s", reflog_action, refname_to_branchname(refname));
	argv_array_push(&cp.args, "checkout");
	argv_array_push(&cp.args, "-q");
	argv_array_push(&cp.args, refname);
	status = run_command(&cp);

	/* reload cache as checkout will have modified it */
	discard_cache();
	read_cache();

	return status;
}

int rebase_common_setup(struct rebase_options *opts, const char *dir)
{
	struct commit *upstream, *onto, *orig_head;

	upstream = lookup_commit_or_die(opts->upstream.hash, "upstream");
	onto = lookup_commit_or_die(opts->onto.hash, opts->onto_name ? opts->onto_name : "onto");
	orig_head = lookup_commit_or_die(opts->orig_head.hash, refname_to_branchname(opts->orig_refname));

	if (opts->autostash)
		die("TODO: autostash not implemented yet");
	else
		rebase_die_on_unclean_worktree(1);

	/*
	 * Now we are rebasing commits upstream..orig_head (or with --root,
	 * everything leading up to orig_head) on top of onto.
	 *
	 * Check if we are already based on onto with linear history, but this
	 * should only be done when upstream and onto are the same.
	 */
	if (upstream == onto && is_linear_history(onto, orig_head)) {
		if (!opts->force) {
			if (opts->orig_refname)
				checkout_refname(opts->orig_refname);
			else
				detach_head(&opts->orig_head, NULL);
			printf_ln(_("Current branch %s is up to date."), refname_to_branchname(opts->orig_refname));
			return 1;
		} else {
			printf_ln(_("Current branch %s is up to date, rebase forced."), refname_to_branchname(opts->orig_refname));
		}
	}

	/* run_pre_rebase_hook */

	/* diffstat */

	/* Detach HEAD and reset the tree */
	printf_ln(_("First, rewinding head to replay your work on top of it..."));
	if (detach_head(&opts->onto, opts->onto_name))
		die(_("could not detach HEAD"));

	update_ref("rebase", "ORIG_HEAD", opts->orig_head.hash, NULL, 0, UPDATE_REFS_DIE_ON_ERR);

	/*
	 * TODO: If the $onto is a proper descendant of the tip of the branch, then
	 * we just fast-forward
	 *
	 * if "$mb" = "$orig_head" then
	 *  say "Fast-forwarded $branch_name to $onto_name"
	 *  move_to_original_branch
	 *  finish_rebase
	 * exit 0
	 */

	return 0;
}

int rebase_output(const struct rebase_options *opts, struct child_process *cp)
{
	struct strbuf sb = STRBUF_INIT;
	int status;

	if (opts->verbose)
		return run_command(cp);

	cp->stdout_to_stderr = 1;
	cp->err = -1;
	if (start_command(cp) < 0)
		return -1;

	if (strbuf_read(&sb, cp->err, 0) < 0) {
		strbuf_release(&sb);
		close(cp->err);
		finish_command(cp);
		return -1;
	}

	close(cp->err);
	status = finish_command(cp);
	if (status)
		printf("%s", sb.buf);
	strbuf_release(&sb);
	return status;
}

void rebase_move_to_original_branch(struct rebase_options *opts)
{
	struct strbuf sb = STRBUF_INIT;
	struct object_id curr_head;

	if (!opts->orig_refname || !starts_with(opts->orig_refname, "refs/"))
		return;

	if (get_sha1("HEAD", curr_head.hash) < 0)
		die("get_sha1() failed");

	strbuf_addf(&sb, "rebase finished: %s onto %s", opts->orig_refname, oid_to_hex(&opts->onto));
	if (update_ref(sb.buf, opts->orig_refname, curr_head.hash, opts->orig_head.hash, 0, UPDATE_REFS_MSG_ON_ERR))
		goto fail;

	strbuf_reset(&sb);
	strbuf_addf(&sb, "rebase finished: returning to %s", opts->orig_refname);
	if (create_symref("HEAD", opts->orig_refname, sb.buf))
		goto fail;

	strbuf_release(&sb);

	return;
fail:
	die(_("Could not move back to %s"), opts->orig_refname);
}

/**
 * finish_rebase
 */
void rebase_common_finish(struct rebase_options *opts, const char *dir)
{
	struct strbuf sb = STRBUF_INIT;

	/* apply_autostash */

	/* git gc --auto || true */

	/* rm -rf "$state_dir" */
	strbuf_addstr(&sb, dir);
	remove_dir_recursively(&sb, 0);
	strbuf_release(&sb);
}
