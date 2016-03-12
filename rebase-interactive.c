#include "cache.h"
#include "rebase-interactive.h"
#include "argv-array.h"
#include "revision.h"
#include "dir.h"
#include "run-command.h"

static int is_empty_commit(struct commit *commit)
{
	if (commit->parents)
		return !oidcmp(&commit->object.oid, &commit->parents->item->object.oid);
	else
		return !hashcmp(commit->object.oid.hash, EMPTY_TREE_SHA1_BIN);
}

GIT_PATH_FUNC(git_path_rebase_interactive_dir, "rebase-merge")

void rebase_interactive_init(struct rebase_interactive *state, const char *dir)
{
	rebase_options_init(&state->opts);
	if (!dir)
		dir = git_path_rebase_interactive_dir();
	state->dir = xstrdup(dir);

	state->todo_file = mkpathdup("%s/git-rebase-todo", state->dir);
	rebase_todo_list_init(&state->todo);
	state->todo_offset = 0;
	state->todo_count = 0;

	state->done_file = mkpathdup("%s/done", state->dir);
	state->done_count = 0;

	state->instruction_format = NULL;
	git_config_get_value("rebase.instructionFormat", &state->instruction_format);
}

void rebase_interactive_release(struct rebase_interactive *state)
{
	rebase_options_release(&state->opts);
	free(state->dir);

	free(state->todo_file);
	rebase_todo_list_clear(&state->todo);

	free(state->done_file);
}

int rebase_interactive_in_progress(const struct rebase_interactive *state)
{
	const char *dir = state ? state->dir : git_path_rebase_interactive_dir();
	struct stat st;

	if (lstat(dir, &st) || !S_ISDIR(st.st_mode))
		return 0;

	if (lstat(mkpath("%s/interactive", dir), &st) || !S_ISREG(st.st_mode))
		return 0;

	return 1;
}

int rebase_interactive_load(struct rebase_interactive *state)
{
	struct rebase_todo_list done;

	/* common rebase options */
	if (rebase_options_load(&state->opts, state->dir) < 0)
		return -1;

	/* todo list */
	rebase_todo_list_clear(&state->todo);
	if (rebase_todo_list_load(&state->todo, state->todo_file, 0) < 0)
		return -1;
	state->todo_offset = 0;
	state->todo_count = rebase_todo_list_count(&state->todo);

	/* done list */
	rebase_todo_list_init(&done);
	if (file_exists(state->done_file) && rebase_todo_list_load(&done, state->done_file, 0) < 0)
		return -1;
	state->done_count = rebase_todo_list_count(&done);
	rebase_todo_list_clear(&done);

	return 0;
}

static int run_command_without_output(const struct rebase_interactive *state,
				      struct child_process *cp)
{
	struct strbuf sb = STRBUF_INIT;
	int status;

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
		fputs(sb.buf, stderr);
	strbuf_release(&sb);
	return status;
}

static int detach_head(const struct rebase_interactive *state, const struct object_id *onto, const char *onto_name)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	const char *reflog_action = getenv("GIT_REFLOG_ACTION");

	if (!reflog_action)
		reflog_action = "";
	if (!onto_name)
		onto_name = oid_to_hex(onto);
	cp.git_cmd = 1;
	argv_array_pushf(&cp.env_array, "GIT_REFLOG_ACTION=%s: checkout %s",
			reflog_action, onto_name);
	argv_array_push(&cp.args, "checkout");
	argv_array_push(&cp.args, oid_to_hex(onto));

	if (run_command_without_output(state, &cp))
		return -1;

	discard_cache();
	read_cache();

	return 0;
}

static int gen_todo_list(struct rebase_interactive *state,
			 const struct object_id *left,
			 const struct object_id *right)
{
	struct rev_info revs;
	struct argv_array args = ARGV_ARRAY_INIT;
	struct pretty_print_context pretty_ctx = {};
	struct commit *commit;
	const char *instruction_format;

	init_revisions(&revs, NULL);
	argv_array_push(&args, "rev-list");
	argv_array_pushl(&args, "--no-merges", "--cherry-pick", NULL);
	argv_array_pushl(&args, "--reverse", "--right-only", "--topo-order", NULL);
	argv_array_pushf(&args, "%s...%s", oid_to_hex(left), oid_to_hex(right));
	setup_revisions(args.argc, args.argv, &revs, NULL);

	if (prepare_revision_walk(&revs))
		die("revision walk setup failed");

	pretty_ctx.fmt = CMIT_FMT_USERFORMAT;
	pretty_ctx.abbrev = revs.abbrev;
	pretty_ctx.output_encoding = get_commit_output_encoding();
	pretty_ctx.color = 0;
	instruction_format = state->instruction_format;
	if (!instruction_format)
		instruction_format = "%s";

	while ((commit = get_revision(&revs))) {
		struct rebase_todo_item *item;
		struct strbuf sb = STRBUF_INIT;

		item = rebase_todo_list_push_empty(&state->todo);
		item->action = REBASE_TODO_PICK;
		oidcpy(&item->oid, &commit->object.oid);

		if (is_empty_commit(commit) && single_parent(commit))
			item->action = REBASE_TODO_NONE;

		format_commit_message(commit, instruction_format, &sb, &pretty_ctx);
		strbuf_setlen(&sb, strcspn(sb.buf, "\n"));
		if (item->action == REBASE_TODO_PICK)
			item->rest = strbuf_detach(&sb, NULL);
		else
			item->rest = xstrfmt("%c pick %s %s", comment_line_char,
					     oid_to_hex(&item->oid), sb.buf);
		strbuf_release(&sb);
	}

	if (!state->todo.nr)
		rebase_todo_list_push_noop(&state->todo);

	reset_revision_walk();
	argv_array_clear(&args);
	return 0;
}

/**
 * Mark the current action as done.
 */
static void mark_action_done(struct rebase_interactive *state)
{
	const struct rebase_todo_item *done_item = &state->todo.items[state->todo_offset++];
	struct strbuf sb = STRBUF_INIT;

	/* update todo file */
	rebase_todo_list_save(&state->todo, state->todo_file, state->todo_offset, 0);

	/* update done file */
	strbuf_add_rebase_todo_item(&sb, done_item, 0);
	append_file(state->done_file, "%s", sb.buf);
	strbuf_release(&sb);

	/* update todo and done counts if item is not none */
	if (done_item->action != REBASE_TODO_NONE) {
		unsigned int total = state->todo_count + state->done_count;

		state->todo_count--;
		state->done_count++;

		printf(_("Rebasing (%u/%u)\r"), state->done_count, total);
	}
}

/**
 * Put the last action marked done at the beginning of the todo list again. If
 * there has not been an action marked done yet, leave the list of items on the
 * todo list unchanged.
 */
static void reschedule_last_action(struct rebase_interactive *state)
{
	struct strbuf sb = STRBUF_INIT;
	const char *last_line;

	if (!state->todo_offset)
		return; /* no action marked done yet */

	/* update todo file */
	rebase_todo_list_save(&state->todo, state->todo_file, --state->todo_offset, 0);

	/* remove the last line from the done file */
	if (strbuf_read_file(&sb, state->done_file, 0) < 0)
		die_errno(_("failed to read %s"), state->done_file);
	last_line = sb.buf + sb.len;
	if (*last_line == '\n')
		last_line--;
	last_line = strrchr(last_line, '\n');
	if (last_line)
		strbuf_setlen(&sb, last_line - sb.buf);
	else
		strbuf_reset(&sb);
	write_file(state->done_file, "%s", sb.buf);
	strbuf_release(&sb);
}

/**
 * Pick a non-merge commit.
 */
static int pick_one_non_merge(struct rebase_interactive *state,
			      const struct object_id *oid, int no_commit)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	int status;

	cp.git_cmd = 1;
	if (state->opts.resolvemsg)
		argv_array_pushf(&cp.env_array, "GIT_CHERRY_PICK_HELP=%s", state->opts.resolvemsg);
	argv_array_push(&cp.args, "cherry-pick");
	argv_array_push(&cp.args, "--allow-empty");
	if (no_commit)
		argv_array_push(&cp.args, "-n");
	else
		argv_array_push(&cp.args, "--ff");
	argv_array_push(&cp.args, oid_to_hex(oid));
	status = run_command_without_output(state, &cp);

	/* Reload index as cherry-pick will have modified it */
	discard_cache();
	read_cache();

	return status;
}

/**
 * Pick a commit.
 */
static int pick_one(struct rebase_interactive *state, const struct object_id *oid,
		    int no_commit)
{
	return pick_one_non_merge(state, oid, no_commit);
}

static void do_pick(struct rebase_interactive *state,
		    const struct rebase_todo_item *item)
{
	int ret;
	struct object_id head;

	if (get_oid("HEAD", &head))
		die("invalid head");

	mark_action_done(state);
	ret = pick_one(state, &item->oid, 0);
	if (ret != 0 && ret != 1)
		reschedule_last_action(state);
	if (ret)
		die(_("Could not apply %s... %s"), oid_to_hex(&item->oid), item->rest);
}

static void do_item(struct rebase_interactive *state)
{
	const struct rebase_todo_item *item = &state->todo.items[state->todo_offset];

	switch (item->action) {
	case REBASE_TODO_NONE:
	case REBASE_TODO_NOOP:
		mark_action_done(state);
		break;
	case REBASE_TODO_PICK:
		do_pick(state, item);
		break;
	default:
		die("BUG: invalid action %d", item->action);
	}
}

static void do_rest(struct rebase_interactive *state)
{
	while (state->todo_offset < state->todo.nr)
		do_item(state);
	rebase_common_finish(&state->opts, state->dir);
}

void rebase_interactive_run(struct rebase_interactive *state)
{
	if (mkdir(state->dir, 0777) < 0 && errno != EEXIST)
		die_errno(_("failed to create directory '%s'"), state->dir);

	write_file(mkpath("%s/interactive", state->dir), "%s", "");
	rebase_options_save(&state->opts, state->dir);

	/* generate initial todo list contents */
	if (gen_todo_list(state, &state->opts.upstream, &state->opts.orig_head) < 0) {
		rebase_common_destroy(&state->opts, state->dir);
		die("could not generate todo list");
	}

	/* open editor on todo list */
	rebase_todo_list_save(&state->todo, state->todo_file, 0, 1);
	if (launch_sequence_editor(state->todo_file, NULL, NULL) < 0) {
		rebase_common_destroy(&state->opts, state->dir);
		die("Could not execute editor");
	}

	/* re-read todo list (which will check the todo list format) */
	rebase_todo_list_clear(&state->todo);
	if (rebase_todo_list_load(&state->todo, state->todo_file, 1) < 0)
		die(_("You can fix this with 'git rebase --edit-todo'"));

	/* count the number of actions in todo list; exit if there are none */
	state->todo_count = rebase_todo_list_count(&state->todo);
	if (!state->todo_count) {
		fprintf_ln(stderr, _("Nothing to do"));
		rebase_common_destroy(&state->opts, state->dir);
		exit(2);
	}

	/* expand todo ids */
	state->todo_count = rebase_todo_list_count(&state->todo);
	rebase_todo_list_save(&state->todo, state->todo_file, 0, 0);

	/* checkout onto */
	if (detach_head(state, &state->opts.onto, state->opts.onto_name) < 0) {
		rebase_common_destroy(&state->opts, state->dir);
		die(_("could not detach HEAD"));
	}

	do_rest(state);
}
