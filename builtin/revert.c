#include "cache.h"
#include "builtin.h"
#include "object.h"
#include "commit.h"
#include "tag.h"
#include "run-command.h"
#include "exec_cmd.h"
#include "utf8.h"
#include "parse-options.h"
#include "cache-tree.h"
#include "diff.h"
#include "revision.h"
#include "rerere.h"
#include "merge-recursive.h"
#include "refs.h"
#include "dir.h"

/*
 * This implements the builtins revert and cherry-pick.
 *
 * Copyright (c) 2007 Johannes E. Schindelin
 *
 * Based on git-revert.sh, which is
 *
 * Copyright (c) 2005 Linus Torvalds
 * Copyright (c) 2005 Junio C Hamano
 */

#define SEQ_DIR		"sequencer"
#define SEQ_HEAD_FILE	"sequencer/head"
#define SEQ_TODO_FILE	"sequencer/todo"

static const char * const revert_usage[] = {
	"git revert [options] <commit-ish>",
	NULL
};

static const char * const cherry_pick_usage[] = {
	"git cherry-pick [options] <commit-ish>",
	NULL
};

static const char *me;
enum replay_action { REVERT, CHERRY_PICK };

struct replay_opts {
	enum replay_action action;

	int reset;
	int contin;

	/* Boolean options */
	int edit;
	int record_origin;
	int no_commit;
	int signoff;
	int allow_ff;
	int allow_rerere_auto;

	int mainline;
	int commit_argc;
	const char **commit_argv;

	/* Merge strategy */
	const char *strategy;
	const char **xopts;
	size_t xopts_nr, xopts_alloc;
};

#define GIT_REFLOG_ACTION "GIT_REFLOG_ACTION"

static char *get_encoding(const char *message);

static const char * const *revert_or_cherry_pick_usage(struct replay_opts *opts)
{
	return opts->action == REVERT ? revert_usage : cherry_pick_usage;
}

static int option_parse_x(const struct option *opt,
			  const char *arg, int unset)
{
	struct replay_opts **opts_ptr = opt->value;
	struct replay_opts *opts = *opts_ptr;

	if (unset)
		return 0;

	ALLOC_GROW(opts->xopts, opts->xopts_nr + 1, opts->xopts_alloc);
	opts->xopts[opts->xopts_nr++] = xstrdup(arg);
	return 0;
}

static void verify_opt_compatible(const char *me, const char *base_opt, ...)
{
	const char *this_opt;
	va_list ap;
	int set;

	va_start(ap, base_opt);
	while ((this_opt = va_arg(ap, const char *))) {
		set = va_arg(ap, int);
		if (set)
			die(_("%s: %s cannot be used with %s"),
				me, this_opt, base_opt);
	}
	va_end(ap);
}

static void verify_opt_mutually_compatible(const char *me, ...)
{
	const char *opt1, *opt2;
	va_list ap;
	int set;

	va_start(ap, me);
	while ((opt1 = va_arg(ap, const char *))) {
		set = va_arg(ap, int);
		if (set)
			break;
	}
	if (!opt1)
		goto ok;
	while ((opt2 = va_arg(ap, const char *))) {
		set = va_arg(ap, int);
		if (set)
			die(_("%s: %s cannot be used with %s"),
				me, opt1, opt2);
	}
ok:
	va_end(ap);
}

static void parse_args(int argc, const char **argv, struct replay_opts *opts)
{
	const char * const * usage_str = revert_or_cherry_pick_usage(opts);
	int noop;
	struct option options[] = {
		OPT_BOOLEAN(0, "reset", &opts->reset, "forget the current operation"),
		OPT_BOOLEAN(0, "continue", &opts->contin, "continue the current operation"),
		OPT_BOOLEAN('n', "no-commit", &opts->no_commit, "don't automatically commit"),
		OPT_BOOLEAN('e', "edit", &opts->edit, "edit the commit message"),
		{ OPTION_BOOLEAN, 'r', NULL, &noop, NULL, "no-op (backward compatibility)",
		  PARSE_OPT_NOARG | PARSE_OPT_HIDDEN, NULL, 0 },
		OPT_BOOLEAN('s', "signoff", &opts->signoff, "add Signed-off-by:"),
		OPT_INTEGER('m', "mainline", &opts->mainline, "parent number"),
		OPT_RERERE_AUTOUPDATE(&opts->allow_rerere_auto),
		OPT_STRING(0, "strategy", &opts->strategy, "strategy", "merge strategy"),
		OPT_CALLBACK('X', "strategy-option", &opts, "option",
			"option for merge strategy", option_parse_x),
		OPT_END(),
		OPT_END(),
		OPT_END(),
	};

	if (opts->action == CHERRY_PICK) {
		struct option cp_extra[] = {
			OPT_BOOLEAN('x', NULL, &opts->record_origin, "append commit name"),
			OPT_BOOLEAN(0, "ff", &opts->allow_ff, "allow fast-forward"),
			OPT_END(),
		};
		if (parse_options_concat(options, ARRAY_SIZE(options), cp_extra))
			die(_("program error"));
	}

	opts->commit_argc = parse_options(argc, argv, NULL, options, usage_str,
					PARSE_OPT_KEEP_ARGV0 |
					PARSE_OPT_KEEP_UNKNOWN);

	/* Check for mutually incompatible command line arguments */
	verify_opt_mutually_compatible(me,
				"--reset", opts->reset,
				"--continue", opts->contin,
				NULL);

	/* Check for incompatible command line arguments */
	if (opts->reset || opts->contin) {
		char *this_operation;
		if (opts->reset)
			this_operation = "--reset";
		else
			this_operation = "--continue";

		verify_opt_compatible(me, this_operation,
				"--no-commit", opts->no_commit,
				"--signoff", opts->signoff,
				"--mainline", opts->mainline,
				"--strategy", opts->strategy ? 1 : 0,
				"--strategy-option", opts->xopts ? 1 : 0,
				"-x", opts->record_origin,
				"--ff", opts->allow_ff,
				NULL);
	}

	else if (opts->commit_argc < 2)
		usage_with_options(usage_str, options);

	if (opts->allow_ff)
		verify_opt_compatible(me, "--ff",
				"--signoff", opts->signoff,
				"--no-commit", opts->no_commit,
				"-x", opts->record_origin,
				"--edit", opts->edit,
				NULL);
	opts->commit_argv = argv;
}

struct commit_message {
	char *parent_label;
	const char *label;
	const char *subject;
	char *reencoded_message;
	const char *message;
};

static int get_message(struct commit *commit, struct commit_message *out)
{
	const char *encoding;
	const char *abbrev, *subject;
	int abbrev_len, subject_len;
	char *q;

	if (!commit->buffer)
		return -1;
	encoding = get_encoding(commit->buffer);
	if (!encoding)
		encoding = "UTF-8";
	if (!git_commit_encoding)
		git_commit_encoding = "UTF-8";

	out->reencoded_message = NULL;
	out->message = commit->buffer;
	if (strcmp(encoding, git_commit_encoding))
		out->reencoded_message = reencode_string(commit->buffer,
					git_commit_encoding, encoding);
	if (out->reencoded_message)
		out->message = out->reencoded_message;

	abbrev = find_unique_abbrev(commit->object.sha1, DEFAULT_ABBREV);
	abbrev_len = strlen(abbrev);

	subject_len = find_commit_subject(out->message, &subject);

	out->parent_label = xmalloc(strlen("parent of ") + abbrev_len +
			      strlen("... ") + subject_len + 1);
	q = out->parent_label;
	q = mempcpy(q, "parent of ", strlen("parent of "));
	out->label = q;
	q = mempcpy(q, abbrev, abbrev_len);
	q = mempcpy(q, "... ", strlen("... "));
	out->subject = q;
	q = mempcpy(q, subject, subject_len);
	*q = '\0';
	return 0;
}

static void free_message(struct commit_message *msg)
{
	free(msg->parent_label);
	free(msg->reencoded_message);
}

static char *get_encoding(const char *message)
{
	const char *p = message, *eol;

	while (*p && *p != '\n') {
		for (eol = p + 1; *eol && *eol != '\n'; eol++)
			; /* do nothing */
		if (!prefixcmp(p, "encoding ")) {
			char *result = xmalloc(eol - 8 - p);
			strlcpy(result, p + 9, eol - 8 - p);
			return result;
		}
		p = eol;
		if (*p == '\n')
			p++;
	}
	return NULL;
}

static void write_cherry_pick_head(struct commit *commit)
{
	int fd;
	struct strbuf buf = STRBUF_INIT;

	strbuf_addf(&buf, "%s\n", sha1_to_hex(commit->object.sha1));

	fd = open(git_path("CHERRY_PICK_HEAD"), O_WRONLY | O_CREAT, 0666);
	if (fd < 0)
		die_errno(_("Could not open '%s' for writing"),
			  git_path("CHERRY_PICK_HEAD"));
	if (write_in_full(fd, buf.buf, buf.len) != buf.len || close(fd))
		die_errno(_("Could not write to '%s'"), git_path("CHERRY_PICK_HEAD"));
	strbuf_release(&buf);
}

static void advise(const char *advice, ...)
{
	va_list params;

	va_start(params, advice);
	vreportf("hint: ", advice, params);
	va_end(params);
}

static void print_advice(void)
{
	char *msg = getenv("GIT_CHERRY_PICK_HELP");

	if (msg) {
		fprintf(stderr, "%s\n", msg);
		/*
		 * A conflict has occured but the porcelain
		 * (typically rebase --interactive) wants to take care
		 * of the commit itself so remove CHERRY_PICK_HEAD
		 */
		unlink(git_path("CHERRY_PICK_HEAD"));
		return;
	}

	advise("after resolving the conflicts, mark the corrected paths");
	advise("with 'git add <paths>' or 'git rm <paths>'");
	advise("and commit the result with 'git commit'");
}

static void write_message(struct strbuf *msgbuf, const char *filename)
{
	static struct lock_file msg_file;

	int msg_fd = hold_lock_file_for_update(&msg_file, filename,
					       LOCK_DIE_ON_ERROR);
	if (write_in_full(msg_fd, msgbuf->buf, msgbuf->len) < 0)
		die_errno(_("Could not write to %s."), filename);
	strbuf_release(msgbuf);
	if (commit_lock_file(&msg_file) < 0)
		die(_("Error wrapping up %s"), filename);
}

static struct tree *empty_tree(void)
{
	struct tree *tree = xcalloc(1, sizeof(struct tree));

	tree->object.parsed = 1;
	tree->object.type = OBJ_TREE;
	pretend_sha1_file(NULL, 0, OBJ_TREE, tree->object.sha1);
	return tree;
}

static int error_dirty_index(const char *me, enum replay_action action)
{
	if (read_cache_unmerged())
		return error_resolve_conflict(me);

	/* Different translation strings for cherry-pick and revert */
	if (action == CHERRY_PICK)
		error(_("Your local changes would be overwritten by %s."), me);
	else
		error(_("Your local changes would be overwritten by %s."), me);

	if (advice_commit_before_merge)
		advise(_("Please, commit your changes or stash them to proceed."));
	return -1;
}

static int fast_forward_to(const unsigned char *to, const unsigned char *from)
{
	struct ref_lock *ref_lock;

	read_cache();
	if (checkout_fast_forward(from, to))
		exit(1); /* the callee should have complained already */
	ref_lock = lock_any_ref_for_update("HEAD", from, 0);
	return write_ref_sha1(ref_lock, to, "cherry-pick");
}

static int do_recursive_merge(struct commit *base, struct commit *next,
			      const char *base_label, const char *next_label,
			      unsigned char *head, struct strbuf *msgbuf,
			      struct replay_opts *opts)
{
	struct merge_options o;
	struct tree *result, *next_tree, *base_tree, *head_tree;
	int clean, index_fd;
	const char **xopt;
	static struct lock_file index_lock;

	index_fd = hold_locked_index(&index_lock, 1);

	read_cache();

	init_merge_options(&o);
	o.ancestor = base ? base_label : "(empty tree)";
	o.branch1 = "HEAD";
	o.branch2 = next ? next_label : "(empty tree)";

	head_tree = parse_tree_indirect(head);
	next_tree = next ? next->tree : empty_tree();
	base_tree = base ? base->tree : empty_tree();

	for (xopt = opts->xopts; xopt != opts->xopts + opts->xopts_nr; xopt++)
		parse_merge_opt(&o, *xopt);

	clean = merge_trees(&o,
			    head_tree,
			    next_tree, base_tree, &result);

	if (active_cache_changed &&
	    (write_cache(index_fd, active_cache, active_nr) ||
	     commit_locked_index(&index_lock)))
		/* TRANSLATORS: %s will be "revert" or "cherry-pick" */
		die(_("%s: Unable to write new index file"), me);
	rollback_lock_file(&index_lock);

	if (!clean) {
		int i;
		strbuf_addstr(msgbuf, "\nConflicts:\n\n");
		for (i = 0; i < active_nr;) {
			struct cache_entry *ce = active_cache[i++];
			if (ce_stage(ce)) {
				strbuf_addch(msgbuf, '\t');
				strbuf_addstr(msgbuf, ce->name);
				strbuf_addch(msgbuf, '\n');
				while (i < active_nr && !strcmp(ce->name,
						active_cache[i]->name))
					i++;
			}
		}
	}

	return !clean;
}

/*
 * If we are cherry-pick, and if the merge did not result in
 * hand-editing, we will hit this commit and inherit the original
 * author date and name.
 * If we are revert, or if our cherry-pick results in a hand merge,
 * we had better say that the current user is responsible for that.
 */
static int run_git_commit(const char *defmsg, struct replay_opts *opts)
{
	/* 6 is max possible length of our args array including NULL */
	const char *args[6];
	int i = 0;

	args[i++] = "commit";
	args[i++] = "-n";
	if (opts->signoff)
		args[i++] = "-s";
	if (!opts->edit) {
		args[i++] = "-F";
		args[i++] = defmsg;
	}
	args[i] = NULL;

	return run_command_v_opt(args, RUN_GIT_CMD);
}

static int do_pick_commit(struct commit *commit, struct replay_opts *opts)
{
	unsigned char head[20];
	struct commit *base, *next, *parent;
	const char *base_label, *next_label;
	struct commit_message msg = { NULL, NULL, NULL, NULL, NULL };
	char *defmsg = NULL;
	struct strbuf msgbuf = STRBUF_INIT;
	int res;

	if (opts->no_commit) {
		/*
		 * We do not intend to commit immediately.  We just want to
		 * merge the differences in, so let's compute the tree
		 * that represents the "current" state for merge-recursive
		 * to work on.
		 */
		if (write_cache_as_tree(head, 0, NULL))
			return error(_("Your index file is unmerged."));
	} else {
		if (get_sha1("HEAD", head))
			return error(_("Can't %s on an unborn branch"), me);
		if (index_differs_from("HEAD", 0))
			return error_dirty_index(me, opts->action);
	}
	discard_cache();

	if (!commit->parents) {
		parent = NULL;
	}
	else if (commit->parents->next) {
		/* Reverting or cherry-picking a merge commit */
		int cnt;
		struct commit_list *p;

		if (!opts->mainline)
			return error(_("Commit %s is a merge but no -m option was given."),
				sha1_to_hex(commit->object.sha1));

		for (cnt = 1, p = commit->parents;
		     cnt != opts->mainline && p;
		     cnt++)
			p = p->next;
		if (cnt != opts->mainline || !p)
			return error(_("Commit %s does not have parent %d"),
				sha1_to_hex(commit->object.sha1), opts->mainline);
		parent = p->item;
	} else if (0 < opts->mainline)
		return error(_("Mainline was specified but commit %s is not a merge."),
			sha1_to_hex(commit->object.sha1));
	else
		parent = commit->parents->item;

	if (opts->allow_ff && parent && !hashcmp(parent->object.sha1, head))
		return fast_forward_to(commit->object.sha1, head);

	if (parent && parse_commit(parent) < 0)
		/* TRANSLATORS: The first %s will be "revert" or
		   "cherry-pick", the second %s a SHA1 */
		return error(_("%s: cannot parse parent commit %s"),
			me, sha1_to_hex(parent->object.sha1));

	if (get_message(commit, &msg) != 0)
		return error(_("Cannot get commit message for %s"),
			sha1_to_hex(commit->object.sha1));

	/*
	 * "commit" is an existing commit.  We would want to apply
	 * the difference it introduces since its first parent "prev"
	 * on top of the current HEAD if we are cherry-pick.  Or the
	 * reverse of it if we are revert.
	 */

	defmsg = git_pathdup("MERGE_MSG");

	if (opts->action == REVERT) {
		base = commit;
		base_label = msg.label;
		next = parent;
		next_label = msg.parent_label;
		strbuf_addstr(&msgbuf, "Revert \"");
		strbuf_addstr(&msgbuf, msg.subject);
		strbuf_addstr(&msgbuf, "\"\n\nThis reverts commit ");
		strbuf_addstr(&msgbuf, sha1_to_hex(commit->object.sha1));

		if (commit->parents && commit->parents->next) {
			strbuf_addstr(&msgbuf, ", reversing\nchanges made to ");
			strbuf_addstr(&msgbuf, sha1_to_hex(parent->object.sha1));
		}
		strbuf_addstr(&msgbuf, ".\n");
	} else {
		const char *p = strstr(msg.message, "\n\n");

		base = parent;
		base_label = msg.parent_label;
		next = commit;
		next_label = msg.label;

		p = p ? p + 2 : sha1_to_hex(commit->object.sha1);
		strbuf_addstr(&msgbuf, p);

		if (opts->record_origin) {
			strbuf_addstr(&msgbuf, "(cherry picked from commit ");
			strbuf_addstr(&msgbuf, sha1_to_hex(commit->object.sha1));
			strbuf_addstr(&msgbuf, ")\n");
		}
		if (!opts->no_commit)
			write_cherry_pick_head(commit);
	}

	if (!opts->strategy || !strcmp(opts->strategy, "recursive") || opts->action == REVERT) {
		res = do_recursive_merge(base, next, base_label, next_label,
					 head, &msgbuf, opts);
		write_message(&msgbuf, defmsg);
	} else {
		struct commit_list *common = NULL;
		struct commit_list *remotes = NULL;

		write_message(&msgbuf, defmsg);

		commit_list_insert(base, &common);
		commit_list_insert(next, &remotes);
		res = try_merge_command(opts->strategy, opts->xopts_nr, opts->xopts,
					common, sha1_to_hex(head), remotes);
		free_commit_list(common);
		free_commit_list(remotes);
	}

	if (res) {
		error(opts->action == REVERT
		      ? _("could not revert %s... %s")
		      : _("could not apply %s... %s"),
		      find_unique_abbrev(commit->object.sha1, DEFAULT_ABBREV),
		      msg.subject);
		print_advice();
		rerere(opts->allow_rerere_auto);
	} else {
		if (!opts->no_commit)
			res = run_git_commit(defmsg, opts);
	}

	free_message(&msg);
	free(defmsg);

	return res;
}

static void prepare_revs(struct rev_info *revs, struct replay_opts *opts)
{
	int argc;

	init_revisions(revs, NULL);
	revs->no_walk = 1;
	if (opts->action != REVERT)
		revs->reverse = 1;

	argc = setup_revisions(opts->commit_argc, opts->commit_argv, revs, NULL);
	if (argc > 1)
		usage(*revert_or_cherry_pick_usage(opts));

	if (prepare_revision_walk(revs))
		die(_("revision walk setup failed"));

	if (!revs->commits)
		die(_("empty commit set passed"));
}

static void read_and_refresh_cache(const char *me, struct replay_opts *opts)
{
	static struct lock_file index_lock;
	int index_fd = hold_locked_index(&index_lock, 0);
	if (read_index_preload(&the_index, NULL) < 0)
		die(_("git %s: failed to read the index"), me);
	refresh_index(&the_index, REFRESH_QUIET|REFRESH_UNMERGED, NULL, NULL, NULL);
	if (the_index.cache_changed) {
		if (write_index(&the_index, index_fd) ||
		    commit_locked_index(&index_lock))
			die(_("git %s: failed to refresh the index"), me);
	}
	rollback_lock_file(&index_lock);
}

static void format_args(struct strbuf *buf, struct replay_opts *opts)
{
	int i;

	if (opts->no_commit)
		strbuf_addstr(buf, "-n ");
	if (opts->edit)
		strbuf_addstr(buf, "-e ");
	if (opts->signoff)
		strbuf_addstr(buf, "-s ");
	if (opts->mainline)
		strbuf_addf(buf, "-m %d ", opts->mainline);
	if (opts->strategy)
		strbuf_addf(buf, "--strategy %s ", opts->strategy);
	if (opts->xopts)
		for (i = 0; i < opts->xopts_nr; i ++)
			strbuf_addf(buf, "-X %s ", opts->xopts[i]);
	if (opts->record_origin)
		strbuf_addstr(buf, "-x ");
	if (opts->allow_ff)
		strbuf_addstr(buf, "--ff ");
}

/*
 * Instruction sheet format ::
 * pick -s 537d2e # revert: Introduce --continue to continue the operation
 * pick 4a15c1 # revert: Introduce --reset to cleanup sequencer data
 */
static void format_todo(struct strbuf *buf, struct commit_list *todo_list,
			struct replay_opts *opts)
{
	struct commit_list *cur = NULL;
	struct commit_message msg = { NULL, NULL, NULL, NULL, NULL };
	const char *sha1_abbrev = NULL;
	const char *action;

	action = (opts->action == REVERT ? "revert" : "pick");
	for (cur = todo_list; cur; cur = cur->next) {
		sha1_abbrev = find_unique_abbrev(cur->item->object.sha1, DEFAULT_ABBREV);
		if (get_message(cur->item, &msg))
			die(_("Cannot get commit message for %s"), sha1_abbrev);
		strbuf_addf(buf, "%s ", action);
		format_args(buf, opts);
		strbuf_addf(buf, "%s # %s\n", sha1_abbrev, msg.subject);
	}
}

static void parse_cmdline_args(struct strbuf *args_to_parse,
			int *argc, const char ***argv)
{
	return;
}

static struct commit *parse_insn_line(const char *start, struct replay_opts *opts)
{
	unsigned char commit_sha1[20];
	char sha1_abbrev[40];
	struct commit *commit;
	enum replay_action action;
	int insn_len = 0;
	char *p, *end;

	int argc = 0;
	const char **argv = NULL;
	struct strbuf args_to_parse = STRBUF_INIT;

	p = (char *) start;
	if (!(p = strchr(p, ' ')))
		return NULL;
	insn_len = p - start;
	if (!(end = strchr(p + 1, '#')))
		return NULL;
	*(end - 1) = '\0';
	strbuf_addstr(&args_to_parse, p + 1);

	/* Parse argc, argv out of args_to_parse */
	/* TODO: Implement parse_cmdline_args! */
	parse_cmdline_args(&args_to_parse, &argc, &argv);
	strbuf_release(&args_to_parse);
	if (argc)
		parse_args(argc, argv, opts);

	/* Copy out the sha1_abbrev explicitly */
	if (!(p = strrchr(p, ' ')))
		return NULL;
	strcpy(sha1_abbrev, p + 1);

	/* Determine the action */
	if (!strncmp(start, "pick", insn_len))
		action = CHERRY_PICK;
	else if (!strncmp(start, "revert", insn_len))
		action = REVERT;
	else
		return NULL;

	/*
	 * Verify that the action matches up with the one in
	 * opts; we don't support arbitrary instructions
	 */
	if (action != opts->action)
		return NULL;

	if ((get_sha1(sha1_abbrev, commit_sha1) < 0)
		|| !(commit = lookup_commit_reference(commit_sha1)))
		return NULL;

	return commit;
}

static void read_populate_todo(struct commit_list **todo_list,
			struct replay_opts *opts)
{
	struct strbuf buf = STRBUF_INIT;
	struct commit_list *new;
	struct commit_list **next;
	struct commit *commit;
	char *p;
	int fd;

	fd = open(git_path(SEQ_TODO_FILE), O_RDONLY);
	if (fd < 0) {
		strbuf_release(&buf);
		die_errno(_("Could not open %s."), git_path(SEQ_TODO_FILE));
	}
	if (strbuf_read(&buf, fd, 0) < buf.len) {
		close(fd);
		strbuf_release(&buf);
		die(_("Could not read %s."), git_path(SEQ_TODO_FILE));
	}
	close(fd);

	next = todo_list;
	for (p = buf.buf; *p;) {
		if (!(commit = parse_insn_line(p, opts))) {
			strbuf_release(&buf);
			die(_("Malformed instruction sheet: %s"), git_path(SEQ_TODO_FILE));
		}
		new = xmalloc(sizeof(struct commit_list));
		new->item = commit;
		*next = new;
		next = &new->next;

		if ((p = strchr(p, '\n')))
			p += 1;
		else
			break;
	}
	*next = NULL;
	strbuf_release(&buf);
}

static void walk_revs_populate_todo(struct commit_list **todo_list,
				struct replay_opts *opts)
{
	struct rev_info revs;
	struct commit *commit;
	struct commit_list *new;
	struct commit_list **next;

	prepare_revs(&revs, opts);

	/* Insert into todo_list in the same order */
	/* NEEDSWORK: Expose this as commit_list_append */
	next = todo_list;
	while ((commit = get_revision(&revs))) {
		new = xmalloc(sizeof(struct commit_list));
		new->item = commit;
		*next = new;
		next = &new->next;
	}
	*next = NULL;
}

static void create_seq_dir(void)
{
	if (file_exists(git_path(SEQ_DIR))) {
		error(_("%s already exists."), git_path(SEQ_DIR));
		advise(_("This usually means that a %s operation is in progress."), me);
		advise(_("Use %s --continue to continue the operation"), me);
		advise(_("or use %s --reset to forget about it"), me);
		die(_("%s failed"), me);
	} else if (mkdir(git_path(SEQ_DIR), 0777) < 0)
		die_errno(_("Could not create sequencer directory '%s'."), git_path(SEQ_DIR));
}

static void save_head(const char *head)
{
	static struct lock_file head_lock;
	struct strbuf buf = STRBUF_INIT;
	int fd;

	fd = hold_lock_file_for_update(&head_lock, git_path(SEQ_HEAD_FILE), LOCK_DIE_ON_ERROR);
	strbuf_addf(&buf, "%s\n", head);
	if (write_in_full(fd, buf.buf, buf.len) < 0)
		die_errno(_("Could not write to %s."), git_path(SEQ_HEAD_FILE));
	if (commit_lock_file(&head_lock) < 0)
		die(_("Error wrapping up %s"), git_path(SEQ_HEAD_FILE));
}

static void save_todo(struct commit_list *todo_list, struct replay_opts *opts)
{
	static struct lock_file todo_lock;
	struct strbuf buf = STRBUF_INIT;
	int fd;

	fd = hold_lock_file_for_update(&todo_lock, git_path(SEQ_TODO_FILE), LOCK_DIE_ON_ERROR);
	format_todo(&buf, todo_list, opts);
	if (write_in_full(fd, buf.buf, buf.len) < 0) {
		strbuf_release(&buf);
		die_errno(_("Could not write to %s."), git_path(SEQ_TODO_FILE));
	}
	if (commit_lock_file(&todo_lock) < 0) {
		strbuf_release(&buf);
		die(_("Error wrapping up %s"), git_path(SEQ_TODO_FILE));
	}
	strbuf_release(&buf);
}

static int cleanup_sequencer_data(void)
{
	static struct strbuf seq_dir = STRBUF_INIT;

	strbuf_addf(&seq_dir, "%s", git_path(SEQ_DIR));
	if (remove_dir_recursively(&seq_dir, 0) < 0) {
		strbuf_release(&seq_dir);
		return error(_("Unable to clean up after successful %s"), me);
	}
	strbuf_release(&seq_dir);
	return 0;
}

static int pick_commits(struct commit_list *todo_list, struct replay_opts *opts)
{
	struct commit_list *cur;
	int res;

	setenv(GIT_REFLOG_ACTION, me, 0);
	if (opts->allow_ff)
		assert(!(opts->signoff || opts->no_commit ||
				opts->record_origin || opts->edit));
	read_and_refresh_cache(me, opts);

	for (cur = todo_list; cur; cur = cur->next) {
		save_todo(cur, opts);
		res = do_pick_commit(cur->item, opts);
		if (res)
			return res;
	}

	/*
	 * Sequence of picks finished successfully; cleanup by
	 * removing the .git/sequencer directory
	 */
	return cleanup_sequencer_data();
}

static int process_continuation(struct replay_opts *opts)
{
	struct commit_list *todo_list = NULL;
	unsigned char sha1[20];

	read_and_refresh_cache(me, opts);

	if (opts->reset) {
		if (!file_exists(git_path(SEQ_TODO_FILE)))
			goto error;
		return cleanup_sequencer_data();
	} else if (opts->contin) {
		if (!file_exists(git_path(SEQ_TODO_FILE)))
			goto error;
		read_populate_todo(&todo_list, opts);

		/* Verify that the conflict has been resolved */
		if (!index_differs_from("HEAD", 0))
			todo_list = todo_list->next;
	} else {
		/*
		 * Start a new cherry-pick/ revert sequence; but
		 * first, make sure that an existing one isn't in
		 * progress
		 */
		if (file_exists(git_path(SEQ_TODO_FILE))) {
			error(_("A %s is already in progress"), me);
			advise(_("Use %s --continue to continue the operation"), me);
			advise(_("or use %s --reset to forget about it"), me);
			return -1;
		}

		walk_revs_populate_todo(&todo_list, opts);
		create_seq_dir();
		if (!get_sha1("HEAD", sha1))
			save_head(sha1_to_hex(sha1));
		save_todo(todo_list, opts);
	}
	return pick_commits(todo_list, opts);
error:
	return error(_("No %s in progress"), me);
}

int cmd_revert(int argc, const char **argv, const char *prefix)
{
	int res;
	struct replay_opts opts;

	memset(&opts, 0, sizeof(struct replay_opts));
	if (isatty(0))
		opts.edit = 1;
	opts.action = REVERT;
	git_config(git_default_config, NULL);
	me = "revert";
	parse_args(argc, argv, &opts);

	/*
	 * Decide what to do depending on the arguments; a fresh
	 * cherry-pick should be handled differently from an existing
	 * one that is being continued
	 */
	res = process_continuation(&opts);
	if (res < 0)
		die(_("%s failed"), me);
	return res;
}

int cmd_cherry_pick(int argc, const char **argv, const char *prefix)
{
	int res;
	struct replay_opts opts;

	memset(&opts, 0, sizeof(struct replay_opts));
	opts.action = CHERRY_PICK;
	git_config(git_default_config, NULL);
	me = "cherry-pick";
	parse_args(argc, argv, &opts);
	res = process_continuation(&opts);
	if (res < 0)
		die(_("%s failed"), me);
	return res;
}
