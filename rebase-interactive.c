#include "cache.h"
#include "rebase-interactive.h"
#include "ident-script.h"
#include "run-command.h"
#include "commit.h"
#include "dir.h"
#include "revision.h"
#include "log-tree.h"
#include "string-list.h"

static void read_file_hex(const char *filename, unsigned char *sha1)
{
	struct strbuf sb = STRBUF_INIT;
	if (strbuf_read_file(&sb, filename, GIT_SHA1_HEXSZ + 1) < 0)
		die(_("could not read file '%s'"), filename);
	if (get_sha1_hex(sb.buf, sha1) < 0)
		die(_("could not parse '%s'"), filename);
	strbuf_release(&sb);
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
	state->msgnum_file = mkpathdup("%s/msgnum", state->dir);
	state->end_file = mkpathdup("%s/end", state->dir);
	state->done_count = 0;

	state->author_file = mkpathdup("%s/author-script", state->dir);
	state->amend_file = mkpathdup("%s/amend", state->dir);
	state->msg_file = mkpathdup("%s/msg", state->dir);

	state->squash_msg_file = mkpathdup("%s/message-squash", state->dir);
	state->fixup_msg_file = mkpathdup("%s/message-fixup", state->dir);

	state->preserve_merges = 0;
	state->rewritten_dir = mkpathdup("%s/rewritten", state->dir);
	state->dropped_dir = mkpathdup("%s/dropped", state->dir);
	oid_array_init(&state->current_commit);

	state->autosquash = 0;

	state->rewritten_list_file = mkpathdup("%s/rewritten-list", state->dir);
	state->rewritten_pending_file = mkpathdup("%s/rewritten-pending", state->dir);
	state->stopped_sha_file = mkpathdup("%s/stopped-sha", state->dir);
	oid_array_init(&state->rewritten_pending);

	oidclr(&state->squash_onto);

	state->instruction_format = NULL;
	git_config_get_value("rebase.instructionFormat", &state->instruction_format);
}

void rebase_interactive_release(struct rebase_interactive *state)
{
	rebase_options_release(&state->opts);
	free(state->dir);

	free(state->todo_file);
	rebase_todo_list_release(&state->todo);

	free(state->done_file);
	free(state->msgnum_file);
	free(state->end_file);

	free(state->author_file);
	free(state->amend_file);
	free(state->msg_file);

	free(state->squash_msg_file);
	free(state->fixup_msg_file);

	free(state->rewritten_dir);
	free(state->dropped_dir);
	oid_array_clear(&state->current_commit);

	free(state->rewritten_list_file);
	free(state->rewritten_pending_file);
	free(state->stopped_sha_file);
	oid_array_clear(&state->rewritten_pending);
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

static const char *state_path(const struct rebase_interactive *state, const char *path)
{
	return mkpath("%s/%s", state->dir, path);
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
	rebase_todo_list_release(&done);

	state->preserve_merges = file_exists(state->rewritten_dir);

	return 0;
}

static void make_patch(const struct rebase_interactive *state, const struct object_id *oid)
{
	struct commit *commit;
	const char *buffer, *ident_line, *msg;
	size_t ident_len;
	struct ident_script author = IDENT_SCRIPT_INIT;
	FILE *fp;

	commit = lookup_commit_reference(oid->hash);
	if (!commit || parse_commit(commit))
		die("invalid commit %s", oid_to_hex(oid));

	write_file(state->stopped_sha_file, "%s", oid_to_hex(oid));

	/* write ident script to state->author_file */
	buffer = logmsg_reencode(commit, NULL, get_commit_output_encoding());
	ident_line = find_commit_header(buffer, "author", &ident_len);
	if (ident_script_from_line(&author, ident_line, ident_len) < 0)
		die(_("invalid ident line: %s"), (char *)xmemdupz(ident_line, ident_len));
	if (!file_exists(state->author_file))
		write_ident_script(&author, state->author_file, "AUTHOR");

	/* write commit message to state->msg_file */
	msg = strstr(buffer, "\n\n");
	if (!msg)
		die(_("unable to parse commit %s"), oid_to_hex(&commit->object.oid));
	if (!file_exists(state->msg_file))
		write_file(state->msg_file, "%s", msg);
	unuse_commit_buffer(commit, buffer);

	/* write patch to "${state->dir}/patch" */
	/* NOTE: we write this last because log_tree_commit() could die but it
	 * is not essential */
	fp = xfopen(state_path(state, "patch"), "w");
	if (!commit->parents) {
		fprintf_ln(fp, "Root commit");
	} else {
		struct rev_info rev_info;

		init_revisions(&rev_info, NULL);
		rev_info.abbrev = 0;
		rev_info.diff = 1;
		rev_info.disable_stdin = 1;
		rev_info.combine_merges = 1;
		rev_info.dense_combined_merges = 1;
		rev_info.no_commit_id = 1;
		rev_info.diffopt.output_format = DIFF_FORMAT_PATCH;
		rev_info.diffopt.use_color = 0;
		rev_info.diffopt.file = fp;
		add_pending_object(&rev_info, &commit->object, "");
		diff_setup_done(&rev_info.diffopt);
		log_tree_commit(&rev_info, commit);
	}
	fclose(fp);
}

static int is_empty_commit(struct commit *commit)
{
	if (commit->parents)
		return !oidcmp(&commit->object.oid, &commit->parents->item->object.oid);
	else
		return !hashcmp(commit->object.oid.hash, EMPTY_TREE_SHA1_BIN);
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

	if (rebase_output(&state->opts, &cp))
		return -1;

	discard_cache();
	read_cache();

	return 0;
}

static const char *rewritten_file(const struct rebase_interactive *state, const struct object_id *oid)
{
	return mkpath("%s/%s", state->rewritten_dir, oid ? oid_to_hex(oid) : "root");
}

static const char *dropped_file(const struct rebase_interactive *state, const struct object_id *oid)
{
	return mkpath("%s/%s", state->dropped_dir, oid ? oid_to_hex(oid) : "root");
}

enum rewrite_status {
	REWRITE_FALSE = 0, /* not marked for rewrite or dropped */
	REWRITE_MARKED, /* marked for rewrite but not rewritten yet */
	REWRITE_REWRITTEN, /* marked for rewrite and rewritten */
	REWRITE_DROPPED, /* dropped and replaced */
	REWRITE_DROPPED_ROOT /* dropped and replaced with root (onto) */
};

static enum rewrite_status rewrite_status(const struct rebase_interactive *state,
		const struct object_id *oid, struct object_id *replacement)
{
	struct strbuf sb = STRBUF_INIT;

	if (strbuf_read_file(&sb, rewritten_file(state, oid), GIT_SHA1_HEXSZ + 1) >= 0) {
		if (!sb.len) {
			strbuf_release(&sb);
			return REWRITE_MARKED;
		}

		if (replacement && get_oid_hex(sb.buf, replacement) < 0)
			die(_("could not parse '%s'"), rewritten_file(state, oid));

		strbuf_release(&sb);
		return REWRITE_REWRITTEN;
	} else if (errno != ENOENT)
		die("failed to read %s", rewritten_file(state, oid));

	if (strbuf_read_file(&sb, dropped_file(state, oid), GIT_SHA1_HEXSZ + 1) >= 0) {
		if (!sb.len) {
			strbuf_release(&sb);
			return REWRITE_DROPPED_ROOT;
		}

		if (replacement && get_oid_hex(sb.buf, replacement) < 0)
			die(_("could not parse '%s'"), dropped_file(state, oid));

		strbuf_release(&sb);
		return REWRITE_DROPPED;
	} else if (errno != ENOENT)
		die("failed to read %s", dropped_file(state, oid));

	strbuf_release(&sb);
	return REWRITE_FALSE;
}

/**
 * Writes replacement to "$rewritten_dir/{oid}"
 *
 * If oid is NULL, write to "$rewritten_dir/root".
 *
 * If replacement is NULL, file is empty.
 */
static void mark_rewrite(struct rebase_interactive *state, const struct object_id *oid,
		const struct object_id *replacement)
{
	write_file(rewritten_file(state, oid), "%s", replacement ? oid_to_hex(replacement) : "");
}

static void mark_dropped(struct rebase_interactive *state, const struct object_id *oid, const struct object_id *replacement)
{
	unlink(rewritten_file(state, oid));
	write_file(dropped_file(state, oid), "%s", replacement ? oid_to_hex(replacement) : "");
}

static int is_rewrite(struct rebase_interactive *state, const struct object_id *oid)
{
	return file_exists(rewritten_file(state, oid));
}

/**
 * Init rewrite list
 *
 * Rewritten contains files for each commit that is reachable by at least one
 * merge base of HEAD and UPSTREAM. They are not necessarily rewritten, but
 * their children might be.
 * This ensures that commits on merged, but otherwise unrelated side branches
 * are left alone. (Think "X" in the man page's example)
 */
static int rewrite_init(struct rebase_interactive *state)
{
	if (mkdir(state->rewritten_dir, 0777) < 0)
		return error(_("failed to create directory '%s'"), state->rewritten_dir);

	if (!state->opts.root) {
		struct commit *orig_head, *upstream;
		struct commit_list *merge_bases, *merge_base;

		orig_head = lookup_commit_reference(state->opts.orig_head.hash);
		if (!orig_head)
			return error(_("could not parse %s"), oid_to_hex(&state->opts.orig_head));

		upstream = lookup_commit_reference(state->opts.upstream.hash);
		if (!upstream)
			return error(_("could not parse %s"), oid_to_hex(&state->opts.upstream));

		merge_bases = get_merge_bases(orig_head, upstream);
		for (merge_base = merge_bases; merge_base; merge_base = merge_base->next)
			mark_rewrite(state, &merge_base->item->object.oid, &state->opts.onto);

		free_commit_list(merge_bases);
	} else {
		mark_rewrite(state, NULL, &state->opts.onto);
	}

	return 0;
}

/**
 * Init dropped list
 */
static int rewrite_cherry_pick(struct rebase_interactive *state, const struct object_id *from, const struct object_id *to)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	struct strbuf sb = STRBUF_INIT;
	struct oid_array not_cherry_picks = OID_ARRAY_INIT;
	struct rebase_todo_list new_todo = REBASE_TODO_LIST_INIT;
	FILE *fp;
	unsigned int i;

	if (mkdir(state->dropped_dir, 0777) < 0)
		return error(_("failed to create directory '%s'"), state->dropped_dir);

	/* Save all non-cherry-picked changes */
	cp.git_cmd = 1;
	cp.out = -1;
	argv_array_push(&cp.args, "rev-list");
	argv_array_push(&cp.args, "--left-right");
	argv_array_push(&cp.args, "--cherry-pick");
	argv_array_pushf(&cp.args, "%s...%s", oid_to_hex(from), oid_to_hex(to));
	if (start_command(&cp) < 0)
		return -1;
	fp = xfdopen(cp.out, "r");
	while (strbuf_getline(&sb, fp) != EOF) {
		struct object_id oid;

		if (sb.buf[0] != '>')
			continue;

		if (get_oid_hex(sb.buf + 1, &oid) < 0)
			die("BUG: invalid line %s", sb.buf);

		oid_array_append(&not_cherry_picks, &oid);
	}
	fclose(fp);
	strbuf_release(&sb);
	if (finish_command(&cp))
		return -1;

	/*
	 * Now all commits and note which ones are missing in not-cherry-picks
	 * and hence being dropped
	 */
	for (i = 0; i < state->todo.nr; i++) {
		const struct rebase_todo_item *item = &state->todo.items[i];

		if (oid_array_find(&not_cherry_picks, &item->oid) >= 0) {
			rebase_todo_list_push(&new_todo, item);
		} else {
			struct commit *commit;
			const struct object_id *first_parent;

			commit = lookup_commit_reference(item->oid.hash);
			if (!commit || parse_commit(commit))
				die("invalid commit %s", oid_to_hex(&item->oid));

			/*
			 * If rev-list --cherry-pick tells us this commit is not
			 * worthwhile, we don't want to track its multiple heads, just
			 * the history of its first-parent for others that will be
			 * rebasing on top of it.
			 */
			first_parent = commit->parents ? &commit->parents->item->object.oid : NULL;
			mark_dropped(state, &item->oid, first_parent);
		}
	}

	rebase_todo_list_swap(&state->todo, &new_todo);
	rebase_todo_list_release(&new_todo);
	oid_array_clear(&not_cherry_picks);
	return 0;
}

/*
 * Generate todo list
 */
static int gen_todo_list(struct rebase_interactive *state, const struct object_id *from, const struct object_id *to)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	struct strbuf sb = STRBUF_INIT;
	FILE *fp;

	if (state->preserve_merges && rewrite_init(state) < 0)
		return error("could not init rewrite");

	cp.git_cmd = 1;
	cp.out = -1;
	argv_array_push(&cp.args, "rev-list");

	/*
	 * If preserve-merges, no cherry-pick because our first pass is to
	 * determine parents to rewrite and skipping dropped commits would
	 * prematurely end our probe.
	 */
	if (!state->preserve_merges)
		argv_array_pushl(&cp.args, "--no-merges", "--cherry-pick", NULL);
	/*
	 * The 'starts_with(sb.buf, ">")' requires %m to parse; the instruction
	 * requires %H to parse
	 */
	argv_array_pushf(&cp.args, "--format=%%m%%H %s", state->instruction_format ? state->instruction_format : "%s");
	argv_array_push(&cp.args, "--format=%m%H %s");
	argv_array_pushl(&cp.args, "--reverse", "--left-right", "--topo-order", NULL);
	argv_array_pushf(&cp.args, "%s...%s", oid_to_hex(from), oid_to_hex(to));
	if (start_command(&cp) < 0)
		return -1;

	fp = xfdopen(cp.out, "r");
	while (strbuf_getline(&sb, fp) != EOF) {
		struct commit *commit;
		struct rebase_todo_item *item;
		const char *line = sb.buf;

		if (*line++ != '>')
			continue;

		item = rebase_todo_list_push_empty(&state->todo);
		item->action = REBASE_TODO_PICK;

		if (get_oid_hex(line, &item->oid) < 0)
			die("BUG: invalid line %s", sb.buf);
		line += GIT_SHA1_HEXSZ;

		while (*line && isspace(*line))
			line++;

		commit = lookup_commit_reference(item->oid.hash);
		if (!commit || parse_commit(commit))
			die("%s: not a commit that can be picked", oid_to_hex(&item->oid));

		if (is_empty_commit(commit) && single_parent(commit))
			item->action = REBASE_TODO_NONE;

		if (state->preserve_merges) {
			int preserve = 1;

			if (!state->opts.root) {
				struct commit_list *parent;
				for (parent = commit->parents; parent; parent = parent->next)
					if (is_rewrite(state, &parent->item->object.oid))
						preserve = 0;
			} else {
				preserve = 0;
			}

			if (!preserve) {
				mark_rewrite(state, &item->oid, NULL);
			} else {
				state->todo.nr--;
				continue;
			}
		}

		if (item->action == REBASE_TODO_PICK) {
			xsnprintf(item->cmd, sizeof(item->cmd), "pick");
			item->rest = xstrdup(line);
		} else {
			xsnprintf(item->cmd, sizeof(item->cmd), "%c", comment_line_char);
			oidclr(&item->oid);
			item->rest = xstrfmt("pick %s %s", oid_to_hex(&item->oid), line);
		}
	}
	fclose(fp);
	strbuf_release(&sb);

	if (finish_command(&cp))
		return -1;

	if (state->preserve_merges && rewrite_cherry_pick(state, from, to) < 0)
		return error("rewrite_cherry_pick() failed");

	if (!state->todo.nr)
		rebase_todo_list_push_noop(&state->todo);

	/* add footer */
	rebase_todo_list_push_empty(&state->todo);
	rebase_todo_list_push_commentf(&state->todo,
		_("Rebase %s onto %s (%u command(s))"),
		"TODO", "TODO", rebase_todo_list_count(&state->todo));
	rebase_todo_list_push_comment(&state->todo,
		_("\nCommands:\n"
		" p, pick = use commit\n"
		" r, reword = use commit, but edit the commit message\n"
		" e, edit = use commit, but stop for amending\n"
		" s, squash = use commit, but meld into previous commit\n"
		" f, fixup = like \"squash\", but discard this commit's log message\n"
		" x, exec = run command (the rest of the line) using shell\n"
		" d, drop = remove commit\n"
		"\n"
		"These lines can be re-ordered; they are executed from top to bottom.\n\n"));
	rebase_todo_list_push_comment(&state->todo, _("If you remove a line here THAT COMMIT WILL BE LOST.\n"));
	rebase_todo_list_push_comment(&state->todo, _("\nHowever, if you remove everything, the rebase will be aborted.\n\n"));

	return 0;
}

/**
 * Mark the current action as done
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

		write_file(state->msgnum_file, "%u", state->done_count);
		write_file(state->end_file, "%u", total);
		printf(_("Rebasing (%u/%u)\r"), state->done_count, total);
	}
}

/*
 * Return next non-noop command
 */
static struct rebase_todo_item *peek_next_command(struct rebase_interactive *state)
{
	unsigned int i;

	for (i = state->todo_offset; i < state->todo.nr; i++) {
		if (state->todo.items[i].action == REBASE_TODO_NONE)
			continue;
		return &state->todo.items[i];
	}

	return NULL;
}

static void flush_rewritten_pending(struct rebase_interactive *state)
{
	struct object_id head;
	size_t i;
	FILE *fp;

	if (get_oid("HEAD", &head))
		die("invalid HEAD");
	fp = xfopen(state->rewritten_list_file, "a");
	for (i = 0; i < state->rewritten_pending.nr; i++)
		fprintf_ln(fp, "%s %s", oid_to_hex(&state->rewritten_pending.oid[i]), oid_to_hex(&head));
	fclose(fp);
	oid_array_clear(&state->rewritten_pending);
	unlink(state->rewritten_pending_file);
}

static void record_in_rewritten(struct rebase_interactive *state, const struct object_id *oid)
{
	const struct rebase_todo_item *next;

	oid_array_append(&state->rewritten_pending, oid);
	append_file(state->rewritten_pending_file, "%s", oid_to_hex(oid));
	next = peek_next_command(state);
	if (!next || (next->action != REBASE_TODO_SQUASH && next->action != REBASE_TODO_FIXUP))
		flush_rewritten_pending(state);
}

static int pick_one_without_merge(struct rebase_interactive *state, const struct object_id *oid, int no_commit)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	int status;
	unsigned int i;

	cp.git_cmd = 1;
	if (state->opts.resolvemsg)
		argv_array_pushf(&cp.env_array, "GIT_CHERRY_PICK_HELP=%s", state->opts.resolvemsg);
	argv_array_push(&cp.args, "cherry-pick");
	if (state->opts.gpg_sign_opt)
		argv_array_push(&cp.args, state->opts.gpg_sign_opt);
	if (state->opts.strategy)
		argv_array_pushf(&cp.args, "--strategy=%s", state->opts.strategy);
	for (i = 0; i < state->opts.strategy_opts.argc; i++)
		argv_array_pushf(&cp.args, "-X%s", state->opts.strategy_opts.argv[i] + 2);
	argv_array_push(&cp.args, "--allow-empty");
	if (no_commit)
		argv_array_push(&cp.args, "-n");
	else if (!state->opts.force)
		argv_array_push(&cp.args, "--ff");
	argv_array_push(&cp.args, oid_to_hex(oid));
	status = rebase_output(&state->opts, &cp);

	/* Reload index as cherry-pick will have modified it */
	discard_cache();
	read_cache();

	return status;
}

static int pick_one_preserving_merges(struct rebase_interactive *state, const struct object_id *oid, int no_commit)
{
	struct commit *commit;
	struct commit_list *parent;
	struct oid_array parents = OID_ARRAY_INIT;
	struct oid_array new_parents = OID_ARRAY_INIT;
	struct object_id head;
	int fast_forward = !no_commit;
	unsigned int i;

	if (get_oid("HEAD", &head))
		return error("could not get HEAD");

	commit = lookup_commit_reference(oid->hash);
	if (!commit || parse_commit(commit))
		return error(_("could not parse %s"), oid_to_hex(oid));

	if (state->current_commit.nr && fast_forward) {
		int i;

		for (i = 0; i < state->current_commit.nr; i++)
			mark_rewrite(state, &state->current_commit.oid[i], &head);

		oid_array_clear(&state->current_commit);
		if (unlink_or_warn(state_path(state, "current-commit")) < 0)
			return error(_("Cannot write current commit's replacement sha1"));
	}

	oid_array_append(&state->current_commit, oid);
	append_file(state_path(state, "current-commit"), "%s", oid_to_hex(oid));

	for (parent = commit->parents; parent; parent = parent->next)
		oid_array_append(&parents, &parent->item->object.oid);

	/* rewrite parents; if none were rewritten, we can fast-forward */
	for (i = 0; i < parents.nr; i++) {
		struct object_id replacement;
		enum rewrite_status status = rewrite_status(state, &parents.oid[i], &replacement);
		switch (status) {
		case REWRITE_FALSE:
			oid_array_append(&new_parents, &parents.oid[i]);
			break;
		case REWRITE_MARKED:
		case REWRITE_REWRITTEN:
			/*
			 * If the todo reordered commits, and our parent is
			 * marked for rewriting, but haven't gotten to yet,
			 * assume the user meant to drop it on top of current
			 * head.
			 */
			if (status == REWRITE_MARKED)
				oidcpy(&replacement, &head);

			if (oidcmp(&parents.oid[i], &replacement))
				fast_forward = 0;
			if (oid_array_find(&new_parents, &replacement) < 0)
				oid_array_append(&new_parents, &replacement);
			break;
		case REWRITE_DROPPED:
			fast_forward = 0;
			oidcpy(&parents.oid[i], &replacement);
			i--;
			continue;
		case REWRITE_DROPPED_ROOT:
			fast_forward = 0;
			oid_array_append(&new_parents, &state->opts.onto);
			break;
		}
	}

	if (!parents.nr) {
		fast_forward = 0;
		oid_array_append(&new_parents, &state->opts.onto);
	}

	oid_array_clear(&parents);

	if (fast_forward) {
		fprintf_ln(stderr, _("Fast-forward to %s"), oid_to_hex(oid));
		if (reset_hard(oid) < 0)
			die(_("Cannot fast-forward to %s"), oid_to_hex(oid));
		oid_array_clear(&new_parents);
		return 0;
	}

	if (!no_commit) {
		if (detach_head(state, &new_parents.oid[0], NULL) < 0)
			die(_("Cannot move HEAD to %s"), oid_to_hex(&new_parents.oid[0]));
	}

	if (new_parents.nr > 1) {
		/* Redo merge */
		struct child_process cp = CHILD_PROCESS_INIT;
		struct ident_script ident_script = IDENT_SCRIPT_INIT;
		const char *buffer, *ident_line, *msg;
		size_t ident_len;

		if (no_commit)
			die(_("Refusing to squash a merge: %s"), oid_to_hex(oid));

		buffer = logmsg_reencode(commit, NULL, get_commit_output_encoding());
		ident_line = find_commit_header(buffer, "author", &ident_len);
		if (ident_script_from_line(&ident_script, ident_line, ident_len) < 0)
			die(_("invalid ident line: %s"), (char *)xmemdupz(ident_line, ident_len));
		msg = strstr(buffer, "\n\n");
		if (!msg)
			die(_("unable to parse commit %s"), oid_to_hex(&commit->object.oid));

		cp.git_cmd = 1;
		argv_array_pushf(&cp.env_array, "GIT_AUTHOR_NAME=%s", ident_script.name);
		argv_array_pushf(&cp.env_array, "GIT_AUTHOR_EMAIL=%s", ident_script.email);
		argv_array_pushf(&cp.env_array, "GIT_AUTHOR_DATE=%s", ident_script.date);
		argv_array_push(&cp.args, "merge");
		if (state->opts.gpg_sign_opt)
			argv_array_push(&cp.args, state->opts.gpg_sign_opt);
		argv_array_push(&cp.args, "--no-log");
		argv_array_push(&cp.args, "--no-ff");
		argv_array_pushl(&cp.args, "-m", msg, NULL);
		for (i = 1; i < new_parents.nr; i++)
			argv_array_push(&cp.args, oid_to_hex(&new_parents.oid[i]));

		if (rebase_output(&state->opts, &cp)) {
			write_file(git_path_merge_msg(), "%s", msg);
			make_patch(state, oid);
			die(_("Error redoing merge %s"), oid_to_hex(oid));
		}

		unuse_commit_buffer(commit, buffer);
		ident_script_release(&ident_script);
	} else {
		return pick_one_without_merge(state, oid, no_commit);
	}

	oid_array_clear(&new_parents);
	return 0;
}

static int pick_one(struct rebase_interactive *state, const struct object_id *oid, int no_commit)
{
	if (state->preserve_merges)
		return pick_one_preserving_merges(state, oid, no_commit);
	else
		return pick_one_without_merge(state, oid, no_commit);
}

static void do_pick(struct rebase_interactive *state, const struct rebase_todo_item *item)
{
	int ret;
	struct object_id head;

	if (get_oid("HEAD", &head))
		die("invalid head");

	/* TODO: comment_for_reflog pick */

	/* NOTE: This is a little different from the shell version */
	if (!oidcmp(&head, &state->squash_onto)) {
		struct child_process cp = CHILD_PROCESS_INIT;

		/*
		 * Set the correct commit message and author info on the
		 * sentinel root.
		 */
		cp.git_cmd = 1;
		argv_array_pushl(&cp.args, "commit", "--allow-empty",
				"--allow-empty-message", "--amend",
				"--no-post-rewrite", "-n", "-q",
				"-C", oid_to_hex(&item->oid), NULL);
		ret = run_command(&cp);
		if (ret)
			goto finish;

		/*
		 * Cherry-pick the original changes without committing. If the
		 * cherry-pick results in conflict, our behaviour is similar to
		 * a standard failed cherry-pick during rebase, with a dirty
		 * index to resolve before manually running git commit --amend
		 * then git rebase --continue.
		 */
		ret = pick_one(state, &item->oid, 1);
		if (ret)
			goto finish;

		/*
		 * Finally, update the sentinel again to include these changes.
		 */
		child_process_init(&cp);
		cp.git_cmd = 1;
		argv_array_pushl(&cp.args, "commit", "--allow-empty",
				"--allow-empty-message", "--amend",
				"--no-post-rewrite", "-n", "-q",
				"-C", oid_to_hex(&item->oid), NULL);
		if (state->opts.gpg_sign_opt)
			argv_array_push(&cp.args, state->opts.gpg_sign_opt);
		ret = run_command(&cp);
	} else {
		ret = pick_one(state, &item->oid, 0);
	}
finish:
	if (ret == 0 || ret == 1)
		mark_action_done(state);
	if (ret) {
		make_patch(state, &item->oid);
		die(_("Could not apply %s... %s"), oid_to_hex(&item->oid), item->rest);
	}
}

static void do_reword(struct rebase_interactive *state, const struct rebase_todo_item *item)
{
	struct child_process cp = CHILD_PROCESS_INIT;

	/* TODO: comment_for_reflog reword */

	/* do_pick */
	do_pick(state, item);

	/* git commit --amend --no-post-rewrite */
	cp.git_cmd = 1;
	argv_array_push(&cp.args, "commit");
	argv_array_push(&cp.args, "--amend");
	argv_array_push(&cp.args, "--no-post-rewrite");
	if (state->opts.gpg_sign_opt)
		argv_array_push(&cp.args, state->opts.gpg_sign_opt);
	if (run_command(&cp)) {
		fprintf_ln(stderr, _("Could not amend commit after successfully picking %s... %s\n"
				"This is most likely due to an empty commit message, or the pre-commit hook\n"
				"failed. If the pre-commit hook failed, you may need to resolve the issue before\n"
				"you are able to reword the commit."), oid_to_hex(&item->oid), item->rest);
		make_patch(state, &item->oid);
		exit(1);
	}
}

static void NORETURN do_edit(struct rebase_interactive *state, const struct rebase_todo_item *item)
{
	/* TODO: comment_for_reflog edit */
	struct object_id head;

	do_pick(state, item);
	fprintf_ln(stderr, "Stopped at %s... %s", oid_to_hex(&item->oid), item->rest);
	if (get_oid("HEAD", &head))
		die("get_oid() failed");
	write_file(state->amend_file, "%s", oid_to_hex(&head));
	make_patch(state, &item->oid);
	exit(0);
}

static unsigned long parse_count_commits(const char *str)
{
	unsigned long count;

	if (!*str++)
		return 0;
	if (!skip_prefix(str, " This is a combination of ", &str))
		return 0;
	count = strtoul(str, (char **)&str, 10);
	if (!skip_prefix(str, " commits.", &str))
		return 0;
	return count;
}

static void update_squash_messages(struct rebase_interactive *state, int fixup, const struct object_id *oid)
{
	FILE *squash_msg_fp;
	struct commit *commit;
	const char *buffer, *msg;
	unsigned long count = 2;

	if (file_exists(state->squash_msg_file)) {
		struct strbuf sb = STRBUF_INIT;
		const char *rest;

		if (strbuf_read_file(&sb, state->squash_msg_file, 0) < 0)
			die(_("could not read %s"), state->squash_msg_file);
		strbuf_complete_line(&sb);
		count = parse_count_commits(sb.buf) + 1U;

		squash_msg_fp = xfopen(state->squash_msg_file, "w");
		fprintf_ln(squash_msg_fp, "%c This is a combination of %lu commits.",
			comment_line_char, count);
		/* discard first line and skip over blank lines */
		rest = strchrnul(sb.buf, '\n');
		while (*rest == '\n')
			rest++;
		fprintf(squash_msg_fp, "%s", rest);
		strbuf_release(&sb);
	} else {
		struct commit *head_commit = lookup_commit_reference_by_name("HEAD");
		const char *head_buffer, *head_msg;
		if (!head_commit)
			die(_("could not lookup commit %s"), "HEAD");
		head_buffer = logmsg_reencode(head_commit, NULL, get_commit_output_encoding());
		head_msg = strstr(head_buffer, "\n\n");
		write_file(state->fixup_msg_file, "%s", head_msg ? head_msg : "");

		squash_msg_fp = xfopen(state->squash_msg_file, "w");
		fprintf(squash_msg_fp, "%c ", comment_line_char);
		fprintf_ln(squash_msg_fp, "This is a combination of 2 commits.");
		fprintf(squash_msg_fp, "%c ", comment_line_char);
		fprintf_ln(squash_msg_fp, _("The first commit's message is:\n"));
		fprintf(squash_msg_fp, "%s", head_msg ? head_msg : "\n");
		unuse_commit_buffer(head_commit, head_buffer);
	}

	commit = lookup_commit_or_die(oid->hash, "");
	buffer = logmsg_reencode(commit, NULL, get_commit_output_encoding());
	msg = strstr(buffer, "\n\n");
	if (!msg)
		msg = "\n";

	if (!fixup) {
		unlink(state->fixup_msg_file);
		fprintf(squash_msg_fp, "\n%c ", comment_line_char);
		fprintf_ln(squash_msg_fp,
			Q_("This is the %lust commit message:",
			"This is the %lu commit message:",
			count), count);
		fprintf(squash_msg_fp, "\n");
		fprintf(squash_msg_fp, "%s", msg);
	} else {
		struct strbuf sb = STRBUF_INIT;
		char *line;

		fprintf(squash_msg_fp, "\n%c ", comment_line_char);
		fprintf_ln(squash_msg_fp,
			Q_("The %lust commit message will be skipped:",
			"The %lu commit message will be skipped:", count),
			count);
		fprintf(squash_msg_fp, "\n");
		strbuf_add_commented_lines(&sb, msg, strlen(msg));
		/* Change the space after the comment character to TAB: */
		for (line = sb.buf; line[0] && line[1] == ' '; line = strchrnul(line, '\n'))
			line[1] = '\t';
		fprintf(squash_msg_fp, "%s", sb.buf);
		strbuf_release(&sb);
	}

	unuse_commit_buffer(commit, buffer);
	fclose(squash_msg_fp);
}

static void do_squash(struct rebase_interactive *state, const struct rebase_todo_item *item, int fixup)
{
	const struct rebase_todo_item *next_item;
	int ret;

	/* TODO: comment_for_reflog */

	if (!state->done_count)
		die(_("cannot '%s' without a previous commit."),
			fixup ? "fixup" : "squash");
	update_squash_messages(state, fixup, &item->oid);

	ret = pick_one(state, &item->oid, 1);
	if (ret == 0 || ret == 1)
		mark_action_done(state);
	if (ret) {
		struct object_id head;
		if (get_oid("HEAD", &head))
			die("get_oid failed");
		write_file(state->amend_file, "%s", oid_to_hex(&head));
		goto fail;
	}

	next_item = peek_next_command(state);
	if (next_item && (next_item->action == REBASE_TODO_SQUASH || next_item->action == REBASE_TODO_FIXUP)) {
		/*
		 * This is an intermediate commit; its message will only be
		 * used in case of trouble. So use the long version:
		 */
		struct child_process cp = CHILD_PROCESS_INIT;

		cp.git_cmd = 1;
		argv_array_push(&cp.args, "commit");
		argv_array_push(&cp.args, "--amend");
		argv_array_push(&cp.args, "--no-verify");
		argv_array_pushl(&cp.args, "-F", state->squash_msg_file, NULL);
		if (state->opts.gpg_sign_opt)
			argv_array_push(&cp.args, state->opts.gpg_sign_opt);
		if (rebase_output(&state->opts, &cp))
			goto fail;
	} else {
		/* This is the final command of this squash/fixup group */
		if (file_exists(state->fixup_msg_file)) {
			struct child_process cp = CHILD_PROCESS_INIT;

			cp.git_cmd = 1;
			argv_array_push(&cp.args, "commit");
			argv_array_push(&cp.args, "--amend");
			argv_array_push(&cp.args, "--no-verify");
			argv_array_pushl(&cp.args, "-F", state->fixup_msg_file, NULL);
			if (state->opts.gpg_sign_opt)
				argv_array_push(&cp.args, state->opts.gpg_sign_opt);
			if (run_command(&cp))
				goto fail;
		} else {
			struct child_process cp = CHILD_PROCESS_INIT;

			if (copy_file(git_path_squash_msg(), state->squash_msg_file, 0666))
				die("copy file failed");
			unlink(git_path_merge_msg());

			cp.git_cmd = 1;
			argv_array_push(&cp.args, "commit");
			argv_array_push(&cp.args, "--amend");
			argv_array_push(&cp.args, "--no-verify");
			argv_array_pushl(&cp.args, "-F", git_path_squash_msg(), NULL);
			argv_array_push(&cp.args, "-e");
			if (state->opts.gpg_sign_opt)
				argv_array_push(&cp.args, state->opts.gpg_sign_opt);
			if (run_command(&cp))
				goto fail;
		}
		unlink(state->squash_msg_file);
		unlink(state->fixup_msg_file);
	}

	/* TODO: record_in_rewritten */

	return;

fail:
	if (rename(state->squash_msg_file, state->msg_file))
		die_errno(_("rename failed"));
	unlink(state->fixup_msg_file);
	unlink(git_path_merge_msg());
	if (copy_file(git_path_merge_msg(), state->msg_file, 0666))
		die_errno("copy failed");
	make_patch(state, &item->oid);
	fprintf(stderr, "\n");
	die(_("Could not apply %s... %s"), oid_to_hex(&item->oid), item->rest);
}

static void do_exec(struct rebase_interactive *state, const struct rebase_todo_item *item)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	int status, dirty;

	/* mark_action_done */
	mark_action_done(state);

	/* Actual execution */
	printf_ln(_("Executing: %s"), item->rest);
	cp.use_shell = 1;
	argv_array_push(&cp.args, item->rest);
	status = run_command(&cp);
	dirty = cache_has_uncommitted_changes(1) || cache_has_unstaged_changes(1);

	if (status) {
		fprintf_ln(stderr, _("Execution failed: %s"), item->rest);
		if (dirty)
			fprintf_ln(stderr, _("and made changes to the index and/or the working tree"));
		fprintf_ln(stderr, _("You can fix the problem, and then run\n"
					"\n"
					"  git rebase --continue\n"));
		if (status == 127) /* command not found */
			status = 1;
		exit(status);
	} else if (dirty) {
		fprintf_ln(stderr, _("Execution succeeded: %s\n"
				"but left changes to the index and/or the working tree.\n"
				"Commit or stash your changes, and then run\n"
				"\n"
				"  git rebase --continue\n"), item->rest);
		exit(1);
	}
}

static void do_item(struct rebase_interactive *state)
{
	const struct rebase_todo_item *item = &state->todo.items[state->todo_offset];

	unlink(state->msg_file);
	unlink(state->author_file);
	unlink(state->amend_file);
	unlink(state->stopped_sha_file);

	switch (item->action) {
	case REBASE_TODO_NONE:
	case REBASE_TODO_NOOP:
	case REBASE_TODO_DROP:
		mark_action_done(state);
		break;
	case REBASE_TODO_PICK:
		do_pick(state, item);
		record_in_rewritten(state, &item->oid);
		break;
	case REBASE_TODO_REWORD:
		do_reword(state, item);
		record_in_rewritten(state, &item->oid);
		break;
	case REBASE_TODO_EDIT:
		do_edit(state, item);
		break;
	case REBASE_TODO_SQUASH:
		do_squash(state, item, 0);
		record_in_rewritten(state, &item->oid);
		break;
	case REBASE_TODO_FIXUP:
		do_squash(state, item, 1);
		record_in_rewritten(state, &item->oid);
		break;
	case REBASE_TODO_EXEC:
		do_exec(state, item);
		break;
	default:
		die("BUG: invalid action %d", item->action);
	}
}

static void rebase_interactive_destroy(struct rebase_interactive *state)
{
	struct strbuf sb = STRBUF_INIT;

	strbuf_addstr(&sb, state->dir);
	remove_dir_recursively(&sb, 0);
	strbuf_release(&sb);
}

static void rebase_interactive_finish(struct rebase_interactive *state)
{
	rebase_move_to_original_branch(&state->opts);
	if (state->opts.verbose) {
		struct child_process cp = CHILD_PROCESS_INIT;
		cp.git_cmd = 1;
		argv_array_push(&cp.args, "diff-tree");
		argv_array_push(&cp.args, "--stat");
		argv_array_pushf(&cp.args, "%s..HEAD", oid_to_hex(&state->opts.orig_head));
		run_command(&cp);
	}
	copy_notes_for_rebase(state->rewritten_list_file);
	rebase_interactive_destroy(state);
}

static void do_rest(struct rebase_interactive *state)
{
	while (state->todo_offset < state->todo.nr)
		do_item(state);
}

struct squash_item {
	const struct rebase_todo_item *todo_item;
	enum rebase_todo_action action;
	char *subject;
	const char *match_subject;
	struct object_id match_oid;
	int used;
};

struct squash_list {
	struct squash_item *items;
	unsigned int nr, alloc;
};

#define SQUASH_LIST_INIT { NULL, 0, 0 }

static void squash_list_release(struct squash_list *list)
{
	unsigned int i;
	for (i = 0; i < list->nr; i++)
		free(list->items[i].subject);
	free(list->items);
}

static struct squash_item *squash_item_push(struct squash_list *list)
{
	struct squash_item *item;
	ALLOC_GROW(list->items, list->nr + 1, list->alloc);
	item = &list->items[list->nr++];
	item->todo_item = NULL;
	item->action = REBASE_TODO_NONE;
	item->subject = NULL;
	item->match_subject = NULL;
	oidclr(&item->match_oid);
	item->used = 0;
	return item;
}

/**
 * Rearrange the todo list that has both "pick sha1 msg" and
 * "pick sha1 fixup!/suqash! msg" appears in it so that the latter comes
 * immediately after the former, and change "pick" to "fixup"/"squash".
 *
 * Note that if the config has received a custom instruction format each log
 * message will be re-retrieved in order to normalize the autosquash
 * arrangement.
 */
static int rearrange_squash(struct rebase_todo_list *todo)
{
	struct squash_list list = SQUASH_LIST_INIT;
	struct rebase_todo_list new_todo = REBASE_TODO_LIST_INIT;
	unsigned int i;
	int ret = 0;

	/* convert todo list to squash list */
	for (i = 0; i < todo->nr; i++) {
		struct squash_item *item = squash_item_push(&list);
		struct commit *commit;
		const char *buffer, *msg;
		struct strbuf sb = STRBUF_INIT;
		item->todo_item = &todo->items[i];

		if (item->todo_item->action != REBASE_TODO_PICK)
			continue;

		commit = lookup_commit_reference(item->todo_item->oid.hash);
		if (!commit || parse_commit(commit)) {
			ret = error(_("invalid commit %s"), oid_to_hex(&item->todo_item->oid));
			goto finish;
		}

		/* get original commit subject */
		buffer = logmsg_reencode(commit, NULL, get_commit_output_encoding());
		msg = strstr(buffer, "\n\n");
		if (!msg) {
			ret = error(_("could not parse commit %s"), oid_to_hex(&item->todo_item->oid));
			unuse_commit_buffer(commit, buffer);
			goto finish;
		}
		strbuf_reset(&sb);
		strbuf_add(&sb, msg+2, strchrnul(msg+2, '\n') - (msg+2));
		strbuf_trim(&sb);
		item->subject = strbuf_detach(&sb, NULL);
		unuse_commit_buffer(commit, buffer);

		if (starts_with(item->subject, "squash! "))
			item->action = REBASE_TODO_SQUASH;
		else if (starts_with(item->subject, "fixup! "))
			item->action = REBASE_TODO_FIXUP;
		else
			continue;

		/* skip all squash! or fixup! */
		item->match_subject = item->subject;
		for (;;) {
			if (starts_with(item->match_subject, "squash! "))
				item->match_subject += strlen("squash! ");
			else if (starts_with(item->match_subject, "fixup! "))
				item->match_subject += strlen("fixup! ");
			else
				break;
		}

		/*
		 * If match_subject is a single word, try to resolve to a full
		 * oid. This allows us to match on both message and on oid
		 * prefix.
		 */
		if (!strchr(item->match_subject, ' '))
			get_oid(item->match_subject, &item->match_oid);

	}

	/* construct new todo list */
	for (i = 0; i < list.nr; i++) {
		struct squash_item *item = &list.items[i];
		unsigned int j;

		if (item->used)
			continue;

		rebase_todo_list_push(&new_todo, item->todo_item);
		item->used = 1;

		if (item->todo_item->action != REBASE_TODO_PICK)
			continue;

		for (j = i + 1; j < list.nr; j++) {
			struct squash_item *squash_item = &list.items[j];
			struct rebase_todo_item *new_item;

			if (squash_item->used || !squash_item->action)
				continue;

			/* Message prefix test */
			if (starts_with(item->subject, squash_item->match_subject))
				; /* emit */
			else if (!oidcmp(&item->todo_item->oid, &squash_item->match_oid))
				; /* emit */
			else
				continue;

			new_item = rebase_todo_list_push(&new_todo, squash_item->todo_item);
			new_item->action = squash_item->action;
			xsnprintf(new_item->cmd, sizeof(new_item->cmd), squash_item->action == REBASE_TODO_SQUASH ? "squash" : "fixup");
			squash_item->used = 1;
		}
	}

	rebase_todo_list_swap(&new_todo, todo);
finish:
	rebase_todo_list_release(&new_todo);
	squash_list_release(&list);
	return ret;
}

static void add_exec_commands(struct rebase_todo_list *todo, const struct string_list *cmds)
{
	struct rebase_todo_list new_todo = REBASE_TODO_LIST_INIT;
	const struct string_list_item *cmd_item;
	unsigned int i;
	int first_pick = 1;

	for (i = 0; i < todo->nr; i++) {
		const struct rebase_todo_item *item = &todo->items[i];

		if (item->action == REBASE_TODO_PICK) {
			if (!first_pick) {
				for_each_string_list_item(cmd_item, cmds)
					rebase_todo_list_push_exec(&new_todo, cmd_item->string);
			}
			first_pick = 0;
		}

		rebase_todo_list_push(&new_todo, item);
	}
	for_each_string_list_item(cmd_item, cmds)
		rebase_todo_list_push_exec(&new_todo, cmd_item->string);

	rebase_todo_list_swap(&new_todo, todo);
	rebase_todo_list_release(&new_todo);
}

/**
 * Skip picking commits whose parents are unchanged
 */
static int skip_unnecessary_picks(struct rebase_interactive *state)
{
	struct rebase_todo_list new_todo = REBASE_TODO_LIST_INIT;
	struct strbuf done = STRBUF_INIT;
	struct commit *onto;
	unsigned int i, done_count = 0;
	int ret = 0;

	onto = lookup_commit_reference(state->opts.onto.hash);
	if (!onto || parse_commit(onto))
		return error(_("could not parse commit %s"), oid_to_hex(&state->opts.onto));

	for (i = 0; i < state->todo.nr; i++) {
		const struct rebase_todo_item *item = &state->todo.items[i];

		/* skip picks whose parent is current onto */
		if (item->action == REBASE_TODO_PICK) {
			struct commit *commit;

			commit = lookup_commit_reference(item->oid.hash);
			if (!commit || parse_commit(commit)) {
				ret = error(_("could not parse commit %s"), oid_to_hex(&item->oid));
				goto finish;
			}

			if (commit->parents && commit->parents->item == onto)
				onto = commit;
			else
				break;
		} else if (item->action != REBASE_TODO_NONE) {
			break;
		}
		strbuf_add_rebase_todo_item(&done, item, 0);
		done_count++;
	}

	/* copy the rest of the items */
	for (; i < state->todo.nr; i++)
		rebase_todo_list_push(&new_todo, &state->todo.items[i]);

	oidcpy(&state->opts.onto, &onto->object.oid);
	write_file(state->done_file, "%s", done.buf);
	strbuf_release(&done);
	rebase_todo_list_swap(&new_todo, &state->todo);
	state->done_count = done_count;

finish:
	rebase_todo_list_release(&new_todo);
	return ret;
}

void rebase_interactive_run(struct rebase_interactive *state, const struct string_list *cmds)
{
	/* Ensure a valid committer ident can be constructed */
	git_committer_info(IDENT_STRICT);

	/* TODO: comment_for_reflog start */

	if (mkdir(state->dir, 0777) < 0 && errno != EEXIST)
		die_errno(_("failed to create directory '%s'"), state->dir);

	write_file(state_path(state, "interactive"), "%s", "");
	rebase_options_save(&state->opts, state->dir);

	if (gen_todo_list(state, &state->opts.upstream, &state->opts.orig_head) < 0)
		die("could not generate todo list");

	if (state->autosquash)
		rearrange_squash(&state->todo);

	/* add_exec_commands */
	if (cmds->nr)
		add_exec_commands(&state->todo, cmds);

	rebase_todo_list_save(&state->todo, state->todo_file, 0, 1);

	/* make todo.backup. scripted version ignores errors. */
	copy_file(state->todo_file, mkpath("%s.backup", state->todo_file), 0666);

	/* open editor on todo list */
	if (launch_sequence_editor(state->todo_file, NULL, NULL) < 0)
		die("Could not execute editor");

	/* re-read todo list (which will check the todo list format) */
	rebase_todo_list_clear(&state->todo);
	if (rebase_todo_list_load(&state->todo, state->todo_file, 0) < 0) {
		die(_("You can fix this with 'git rebase --edit-todo'"));
	}

	/* has_action, return 2 */
	state->todo_count = rebase_todo_list_count(&state->todo);
	if (!state->todo_count) {
		fprintf_ln(stderr, _("Nothing to do"));
		rebase_interactive_destroy(state);
		exit(2);
	}

	/* TODO: check_todo_list */

	if (!state->preserve_merges && !state->opts.force && skip_unnecessary_picks(state) < 0) {
		rebase_interactive_destroy(state);
		die(_("failed to skip unnecessary picks"));
	}

	/* expand_todo_ids */
	state->todo_count = rebase_todo_list_count(&state->todo);
	rebase_todo_list_save(&state->todo, state->todo_file, 0, 0);

	/* checkout_onto */
	if (detach_head(state, &state->opts.onto, state->opts.onto_name) < 0) {
		rebase_interactive_destroy(state);
		die(_("could not detach HEAD"));
	}

	/* do_rest */
	do_rest(state);

	rebase_interactive_finish(state);
}

void rebase_interactive_continue(struct rebase_interactive *state)
{
	/* Do we have anything to commit? */
	if (!cache_has_uncommitted_changes(0)) {
		/* Nothing to commit -- skip this commit */
		if (unlink(git_path_cherry_pick_head()) && errno != ENOENT)
			die(_("Could not remove CHERRY_PICK_HEAD"));
	} else {
		struct ident_script author = IDENT_SCRIPT_INIT;

		if (!file_exists(state->author_file))
			die(_("You have staged changes in your working tree. If these changes are meant to be\n"
				"squashed into the previous commit, run:\n"
				"\n"
				"    git commit --amend\n"
				"\n"
				"If they are meant to go into a new commit, run:\n"
				"\n"
				"    git commit\n"
				"\n"
				"In both cases, once you're done, continue with:\n"
				"\n"
				"    git rebase --continue\n"));

		if (read_ident_script(&author, state->author_file, "AUTHOR") < 0)
			die(_("Error trying to find the author identity to amend commit."));

		if (file_exists(state->amend_file)) {
			unsigned char amend[GIT_SHA1_HEXSZ];
			unsigned char curr_head[GIT_SHA1_HEXSZ];
			struct child_process cp = CHILD_PROCESS_INIT;

			if (get_sha1("HEAD", curr_head))
				die("could not get HEAD");
			read_file_hex(state->amend_file, amend);

			if (hashcmp(curr_head, amend))
				die(_("You have uncommitted changes in your working tree. Please, commit them\n"
					"first and then run 'git rebase --continue' again."));

			cp.git_cmd = 1;
			argv_array_pushl(&cp.args, "commit", "--amend",
					"--no-verify", "-F", state->msg_file,
					"-e", NULL);
			if (state->opts.gpg_sign_opt)
				argv_array_push(&cp.args, state->opts.gpg_sign_opt);
			if (run_command(&cp))
				die(_("Could not commit staged changes."));
		} else {
			struct child_process cp = CHILD_PROCESS_INIT;

			cp.git_cmd = 1;
			argv_array_pushl(&cp.args, "commit", "--no-verify",
					"-F", state->msg_file, "-e", NULL);
			if (state->opts.gpg_sign_opt)
				argv_array_push(&cp.args, state->opts.gpg_sign_opt);
			if (run_command(&cp))
				die(_("Could not commit staged changes."));
		}

		ident_script_release(&author);
	}

	if (file_exists(state->stopped_sha_file)) {
		struct object_id oid;
		read_file_hex(state->stopped_sha_file, oid.hash);
		record_in_rewritten(state, &oid);
	}

	rebase_die_on_unclean_worktree(1);

	do_rest(state);
	rebase_interactive_finish(state);
}

void rebase_interactive_skip(struct rebase_interactive *state)
{
	/* TODO: rerere clear */

	/* do_rest */
	do_rest(state);
	rebase_interactive_finish(state);
}
