#include "cache.h"
#include "rebase-am.h"
#include "run-command.h"
#include "dir.h"

GIT_PATH_FUNC(git_path_rebase_am_dir, "rebase-apply");

int rebase_am_in_progress(const struct rebase_am *state)
{
	const char *dir = state ? state->dir : git_path_rebase_am_dir();
	struct stat st;

	return !lstat(dir, &st) && S_ISDIR(st.st_mode);
}

void rebase_am_init(struct rebase_am *state, const char *dir)
{
	if (!dir)
		dir = git_path_rebase_am_dir();
	rebase_options_init(&state->opts);
	state->dir = xstrdup(dir);
}

void rebase_am_release(struct rebase_am *state)
{
	rebase_options_release(&state->opts);
	free(state->dir);
}

int rebase_am_load(struct rebase_am *state)
{
	if (rebase_options_load(&state->opts, state->dir) < 0)
		return -1;

	return 0;
}

void rebase_am_save(struct rebase_am *state)
{
	rebase_options_save(&state->opts, state->dir);
}

static int run_format_patch(const char *patches, const struct object_id *left,
		const struct object_id *right)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	int ret;

	cp.git_cmd = 1;
	cp.out = xopen(patches, O_WRONLY | O_CREAT, 0777);
	argv_array_push(&cp.args, "format-patch");
	argv_array_push(&cp.args, "-k");
	argv_array_push(&cp.args, "--stdout");
	argv_array_push(&cp.args, "--full-index");
	argv_array_push(&cp.args, "--cherry-pick");
	argv_array_push(&cp.args, "--right-only");
	argv_array_push(&cp.args, "--src-prefix=a/");
	argv_array_push(&cp.args, "--dst-prefix=b/");
	argv_array_push(&cp.args, "--no-renames");
	argv_array_push(&cp.args, "--no-cover-letter");
	argv_array_pushf(&cp.args, "%s..%s", oid_to_hex(left), oid_to_hex(right));

	ret = run_command(&cp);
	close(cp.out);
	return ret;
}

static int run_am(const struct rebase_am *state, const char *patches, const char **am_opts)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	int ret;

	cp.git_cmd = 1;
	cp.in = xopen(patches, O_RDONLY);
	argv_array_push(&cp.args, "am");
	argv_array_pushv(&cp.args, am_opts);
	argv_array_push(&cp.args, "--rebasing");
	if (state->opts.resolvemsg)
		argv_array_pushf(&cp.args, "--resolvemsg=%s", state->opts.resolvemsg);
	if (state->opts.gpg_sign_opt)
		argv_array_push(&cp.args, state->opts.gpg_sign_opt);

	ret = run_command(&cp);
	close(cp.in);

	/* Reload cache as git-am will have modified it */
	discard_cache();
	read_cache();

	return ret;
}

static void rebase_am_finish(struct rebase_am *state)
{
	rebase_common_finish(&state->opts, state->dir);
}

void rebase_am_run(struct rebase_am *state, const char **am_opts)
{
	char *patches;
	int ret;

	if (rebase_common_setup(&state->opts, state->dir)) {
		rebase_am_finish(state);
		return;
	}

	patches = git_pathdup("rebased-patches");
	ret = run_format_patch(patches, &state->opts.upstream, &state->opts.orig_head);
	if (ret) {
		unlink_or_warn(patches);
		fprintf_ln(stderr, _("\ngit encountered an error while preparing the patches to replay\n"
			"these revisions:\n"
			"\n"
			"    %s..%s\n"
			"\n"
			"As a result, git cannot rebase them."),
				oid_to_hex(&state->opts.upstream),
				oid_to_hex(&state->opts.orig_head));
		exit(ret);
	}

	ret = run_am(state, patches, am_opts);
	unlink_or_warn(patches);
	if (ret) {
		rebase_am_save(state);
		exit(ret);
	}

	free(patches);
	rebase_move_to_original_branch(&state->opts);
	rebase_am_finish(state);
}

void rebase_am_continue(struct rebase_am *state)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	int ret;

	cp.git_cmd = 1;
	argv_array_pushl(&cp.args, "am", "--resolved", NULL);
	if (state->opts.resolvemsg)
		argv_array_pushf(&cp.args, "--resolvemsg=%s", state->opts.resolvemsg);
	if (state->opts.gpg_sign_opt)
		argv_array_push(&cp.args, state->opts.gpg_sign_opt);

	ret = run_command(&cp);
	if (ret)
		exit(ret);

	rebase_move_to_original_branch(&state->opts);
	rebase_am_finish(state);
}

void rebase_am_skip(struct rebase_am *state)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	int ret;

	cp.git_cmd = 1;
	argv_array_pushl(&cp.args, "am", "--skip", NULL);
	if (state->opts.resolvemsg)
		argv_array_pushf(&cp.args, "--resolvemsg=%s", state->opts.resolvemsg);

	ret = run_command(&cp);
	if (ret)
		exit(ret);

	rebase_move_to_original_branch(&state->opts);
	rebase_am_finish(state);
}
