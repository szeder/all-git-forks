#include "cache.h"
#include "rebase-am.h"
#include "run-command.h"

GIT_PATH_FUNC(git_path_rebase_am_dir, "rebase-apply");

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

int rebase_am_in_progress(const struct rebase_am *state)
{
	const char *dir = state ? state->dir : git_path_rebase_am_dir();
	struct stat st;

	return !lstat(dir, &st) && S_ISDIR(st.st_mode);
}

int rebase_am_load(struct rebase_am *state)
{
	if (rebase_options_load(&state->opts, state->dir) < 0)
		return -1;

	return 0;
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
	argv_array_pushf(&cp.args, "%s...%s", oid_to_hex(left), oid_to_hex(right));

	ret = run_command(&cp);
	close(cp.out);
	return ret;
}

static int run_am(const struct rebase_am *state, const char *patches)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	int ret;

	cp.git_cmd = 1;
	cp.in = xopen(patches, O_RDONLY);
	argv_array_push(&cp.args, "am");
	argv_array_push(&cp.args, "--rebasing");
	if (state->opts.resolvemsg)
		argv_array_pushf(&cp.args, "--resolvemsg=%s", state->opts.resolvemsg);

	ret = run_command(&cp);
	close(cp.in);
	return ret;
}

void rebase_am_run(struct rebase_am *state)
{
	char *patches;
	int ret;

	rebase_common_setup(&state->opts, state->dir);

	patches = git_pathdup("rebased-patches");
	ret = run_format_patch(patches, &state->opts.upstream, &state->opts.orig_head);
	if (ret) {
		unlink_or_warn(patches);
		fprintf_ln(stderr, _("\ngit encountered an error while preparing the patches to replay\n"
			"these revisions:\n"
			"\n"
			"    %s...%s\n"
			"\n"
			"As a result, git cannot rebase them."),
				oid_to_hex(&state->opts.upstream),
				oid_to_hex(&state->opts.orig_head));
		exit(ret);
	}

	ret = run_am(state, patches);
	unlink_or_warn(patches);
	if (ret) {
		rebase_options_save(&state->opts, state->dir);
		exit(ret);
	}

	free(patches);
	rebase_common_finish(&state->opts, state->dir);
}
