#include "cache.h"
#include "rebase-merge.h"
#include "run-command.h"
#include "dir.h"
#include "revision.h"

GIT_PATH_FUNC(git_path_rebase_merge_dir, "rebase-merge");

void rebase_merge_init(struct rebase_merge *state, const char *dir)
{
	if (!dir)
		dir = git_path_rebase_merge_dir();
	rebase_options_init(&state->opts);
	state->dir = xstrdup(dir);
	state->msgnum = 0;
	state->end = 0;
	state->prec = 4;
}

void rebase_merge_release(struct rebase_merge *state)
{
	rebase_options_release(&state->opts);
	free(state->dir);
}

int rebase_merge_in_progress(const struct rebase_merge *state)
{
	const char *dir = state ? state->dir : git_path_rebase_merge_dir();
	struct stat st;

	if (lstat(dir, &st) || !S_ISDIR(st.st_mode))
		return 0;

	if (file_exists(mkpath("%s/interactive", dir)))
		return 0;

	return 1;
}

static const char *state_path(const struct rebase_merge *state, const char *filename)
{
	return mkpath("%s/%s", state->dir, filename);
}

static int read_state_file(const struct rebase_merge *state, const char *filename, struct strbuf *sb)
{
	const char *path = state_path(state, filename);
	if (strbuf_read_file(sb, path, 0) < 0)
		return error(_("could not read file %s"), path);
	strbuf_trim(sb);
	return 0;
}

static int read_state_ui(const struct rebase_merge *state, const char *filename, unsigned int *ui)
{
	struct strbuf sb = STRBUF_INIT;
	if (read_state_file(state, filename, &sb) < 0) {
		strbuf_release(&sb);
		return -1;
	}
	if (strtoul_ui(sb.buf, 10, ui) < 0) {
		strbuf_release(&sb);
		return error(_("could not parse %s"), state_path(state, filename));
	}
	strbuf_release(&sb);
	return 0;
}

static int read_state_oid(const struct rebase_merge *state, const char *filename, struct object_id *oid)
{
	struct strbuf sb = STRBUF_INIT;
	if (read_state_file(state, filename, &sb) < 0) {
		strbuf_release(&sb);
		return -1;
	}
	if (sb.len != GIT_SHA1_HEXSZ || get_oid_hex(sb.buf, oid)) {
		strbuf_release(&sb);
		return error(_("could not parse %s"), state_path(state, filename));
	}
	strbuf_release(&sb);
	return 0;
}

static int read_state_msgnum(const struct rebase_merge *state, unsigned int msgnum, struct object_id *oid)
{
	char *filename = xstrfmt("cmt.%u", msgnum);
	int ret = read_state_oid(state, filename, oid);
	free(filename);
	return ret;
}

int rebase_merge_load(struct rebase_merge *state)
{
	struct strbuf sb = STRBUF_INIT;

	if (rebase_options_load(&state->opts, state->dir) < 0)
		return -1;

	if (read_state_file(state, "onto_name", &sb) < 0)
		return -1;
	free(state->opts.onto_name);
	state->opts.onto_name = strbuf_detach(&sb, NULL);

	if (read_state_ui(state, "msgnum", &state->msgnum) < 0)
		return -1;

	if (read_state_ui(state, "end", &state->end) < 0)
		return -1;

	strbuf_release(&sb);
	return 0;
}

static void write_state_text(const struct rebase_merge *state, const char *filename, const char *text)
{
	write_file(state_path(state, filename), "%s", text);
}

static void write_state_ui(const struct rebase_merge *state, const char *filename, unsigned int ui)
{
	write_file(state_path(state, filename), "%u", ui);
}

static void write_state_oid(const struct rebase_merge *state, const char *filename, const struct object_id *oid)
{
	write_file(state_path(state, filename), "%s", oid_to_hex(oid));
}

static void rebase_merge_finish(struct rebase_merge *state)
{
	rebase_common_finish(&state->opts, state->dir);
	printf_ln(_("All done."));
}

/**
 * Setup commits to be rebased.
 */
static unsigned int setup_commits(const char *dir, const struct object_id *upstream, const struct object_id *head)
{
	struct rev_info revs;
	struct argv_array args = ARGV_ARRAY_INIT;
	struct commit *commit;
	unsigned int msgnum = 0;

	init_revisions(&revs, NULL);
	argv_array_pushl(&args, "rev-list", "--reverse", "--no-merges", NULL);
	argv_array_pushf(&args, "%s..%s", oid_to_hex(upstream), oid_to_hex(head));
	setup_revisions(args.argc, args.argv, &revs, NULL);
	if (prepare_revision_walk(&revs))
		die("revision walk setup failed");
	while ((commit = get_revision(&revs)))
		write_file(mkpath("%s/cmt.%u", dir, ++msgnum), "%s", oid_to_hex(&commit->object.oid));
	reset_revision_walk();
	argv_array_clear(&args);
	return msgnum;
}

/**
 * Merge HEAD with oid
 */
static void do_merge(struct rebase_merge *state, unsigned int msgnum, const struct object_id *oid)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	int ret;

	cp.git_cmd = 1;
	argv_array_pushf(&cp.env_array, "GITHEAD_%s=HEAD~%u", oid_to_hex(oid), state->end - msgnum);
	argv_array_pushf(&cp.env_array, "GITHEAD_HEAD=%s", state->opts.onto_name ? state->opts.onto_name : oid_to_hex(&state->opts.onto));
	argv_array_push(&cp.args, "merge-recursive");
	argv_array_pushf(&cp.args, "%s^", oid_to_hex(oid));
	argv_array_push(&cp.args, "--");
	argv_array_push(&cp.args, "HEAD");
	argv_array_push(&cp.args, oid_to_hex(oid));
	ret = run_command(&cp);
	switch (ret) {
	case 0:
		break;
	case 1:
		if (state->opts.resolvemsg)
			fprintf_ln(stderr, "%s", state->opts.resolvemsg);
		exit(1);
	case 2:
		fprintf_ln(stderr, _("Strategy: recursive failed, try another"));
		if (state->opts.resolvemsg)
			fprintf_ln(stderr, "%s", state->opts.resolvemsg);
		exit(1);
	default:
		fprintf_ln(stderr, _("Unknown exit code (%d) from command"), ret);
		exit(1);
	}

	discard_cache();
	read_cache();
}

/**
 * Commit index
 */
static void do_commit(struct rebase_merge *state, unsigned int msgnum, const struct object_id *oid)
{
	struct child_process cp = CHILD_PROCESS_INIT;

	if (!cache_has_uncommitted_changes()) {
		printf_ln(_("Already applied: %0*d"), state->prec, msgnum);
		return;
	}

	cp.git_cmd = 1;
	argv_array_push(&cp.args, "commit");
	argv_array_push(&cp.args, "--no-verify");
	argv_array_pushl(&cp.args, "-C", oid_to_hex(oid), NULL);
	if (run_command(&cp)) {

		fprintf_ln(stderr, _("Commit failed, please do not call \"git commit\"\n"
					"directly, but instead do one of the following:"));
		if (state->opts.resolvemsg)
			fprintf_ln(stderr, "%s", state->opts.resolvemsg);
		exit(1);
	}
	printf_ln(_("Committed: %0*d"), state->prec, msgnum);
}

static void do_rest(struct rebase_merge *state)
{
	while (state->msgnum <= state->end) {
		struct object_id oid;

		if (read_state_msgnum(state, state->msgnum, &oid) < 0)
			die("could not read msgnum commit");
		write_state_oid(state, "current", &oid);
		do_merge(state, state->msgnum, &oid);
		do_commit(state, state->msgnum, &oid);
		write_state_ui(state, "msgnum", ++state->msgnum);
	}

	rebase_merge_finish(state);
}

void rebase_merge_run(struct rebase_merge *state)
{
	rebase_common_setup(&state->opts, state->dir);

	if (mkdir(state->dir, 0777) < 0 && errno != EEXIST)
		die_errno(_("failed to create directory '%s'"), state->dir);

	rebase_options_save(&state->opts, state->dir);
	write_state_text(state, "onto_name", state->opts.onto_name ? state->opts.onto_name : oid_to_hex(&state->opts.onto));

	state->msgnum = 1;
	write_state_ui(state, "msgnum", state->msgnum);

	state->end = setup_commits(state->dir, &state->opts.upstream, &state->opts.orig_head);
	write_state_ui(state, "end", state->end);

	do_rest(state);
}
