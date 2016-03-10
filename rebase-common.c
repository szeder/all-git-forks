#include "cache.h"
#include "rebase-common.h"
#include "dir.h"
#include "run-command.h"
#include "refs.h"
#include "lockfile.h"

void refresh_and_write_cache(unsigned int flags)
{
	struct lock_file *lock_file = xcalloc(1, sizeof(struct lock_file));

	hold_locked_index(lock_file, 1);
	refresh_cache(flags);
	if (write_locked_index(&the_index, lock_file, COMMIT_LOCK))
		die(_("unable to write index file"));
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

static int detach_head(const struct object_id *commit, const char *onto_name)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	int status;
	const char *reflog_action = getenv("GIT_REFLOG_ACTION");
	if (!reflog_action || !*reflog_action)
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

void rebase_common_setup(struct rebase_options *opts, const char *dir)
{
	/* Detach HEAD and reset the tree */
	printf_ln(_("First, rewinding head to replay your work on top of it..."));
	if (detach_head(&opts->onto, opts->onto_name))
		die(_("could not detach HEAD"));
	update_ref("rebase", "ORIG_HEAD", opts->orig_head.hash, NULL, 0,
			UPDATE_REFS_DIE_ON_ERR);
}

void rebase_common_destroy(struct rebase_options *opts, const char *dir)
{
	struct strbuf sb = STRBUF_INIT;
	strbuf_addstr(&sb, dir);
	remove_dir_recursively(&sb, 0);
	strbuf_release(&sb);
}

static void move_to_original_branch(struct rebase_options *opts)
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

void rebase_common_finish(struct rebase_options *opts, const char *dir)
{
	const char *argv_gc_auto[] = {"gc", "--auto", NULL};

	move_to_original_branch(opts);
	close_all_packs();
	run_command_v_opt(argv_gc_auto, RUN_GIT_CMD);
	rebase_common_destroy(opts, dir);
}
