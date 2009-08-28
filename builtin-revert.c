#include "cache.h"
#include "builtin.h"
#include "commit.h"
#include "tag.h"
#include "exec_cmd.h"
#include "utf8.h"
#include "parse-options.h"
#include "diff.h"
#include "revision.h"
#include "rerere.h"
#include "pick.h"

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

static const char * const revert_usage[] = {
	"git revert [options] <commit-ish>",
	NULL
};

static const char * const cherry_pick_usage[] = {
	"git cherry-pick [options] <commit-ish>",
	NULL
};

static int edit, no_commit, mainline, signoff;
static int flags;
static struct commit *commit;

#define GIT_REFLOG_ACTION "GIT_REFLOG_ACTION"

static void parse_args(int argc, const char **argv)
{
	const char * const * usage_str =
		flags & PICK_REVERSE ? revert_usage : cherry_pick_usage;
	unsigned char sha1[20];
	const char *arg;
	int noop;
	struct option options[] = {
		OPT_BOOLEAN('n', "no-commit", &no_commit, "don't automatically commit"),
		OPT_BOOLEAN('e', "edit", &edit, "edit the commit message"),
		OPT_BIT('x', NULL, &flags, "append commit name when cherry-picking", PICK_ADD_NOTE),
		OPT_BOOLEAN('r', NULL, &noop, "no-op (backward compatibility)"),
		OPT_BOOLEAN('s', "signoff", &signoff, "add Signed-off-by:"),
		OPT_INTEGER('m', "mainline", &mainline, "parent number"),
		OPT_END(),
	};

	if (parse_options(argc, argv, NULL, options, usage_str, 0) != 1)
		usage_with_options(usage_str, options);
	arg = argv[0];

	if (get_sha1(arg, sha1))
		die ("Cannot find '%s'", arg);
	commit = (struct commit *)parse_object(sha1);
	if (!commit)
		die ("Could not find %s", sha1_to_hex(sha1));
	if (commit->object.type == OBJ_TAG) {
		commit = (struct commit *)
			deref_tag((struct object *)commit, arg, strlen(arg));
	}
	if (commit->object.type != OBJ_COMMIT)
		die ("'%s' does not point to a commit", arg);
}

static char *get_encoding(const char *message)
{
	const char *p = message, *eol;

	if (!p)
		return NULL;
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

static void set_author_ident_env(const char *message)
{
	const char *p = message;
	if (!p)
		die ("Could not read commit message of %s",
				sha1_to_hex(commit->object.sha1));
	while (*p && *p != '\n') {
		const char *eol;

		for (eol = p; *eol && *eol != '\n'; eol++)
			; /* do nothing */
		if (!prefixcmp(p, "author ")) {
			char *line, *pend, *email, *timestamp;

			p += 7;
			line = xmemdupz(p, eol - p);
			email = strchr(line, '<');
			if (!email)
				die ("Could not extract author email from %s",
					sha1_to_hex(commit->object.sha1));
			if (email == line)
				pend = line;
			else
				for (pend = email; pend != line + 1 &&
						isspace(pend[-1]); pend--);
					; /* do nothing */
			*pend = '\0';
			email++;
			timestamp = strchr(email, '>');
			if (!timestamp)
				die ("Could not extract author time from %s",
					sha1_to_hex(commit->object.sha1));
			*timestamp = '\0';
			for (timestamp++; *timestamp && isspace(*timestamp);
					timestamp++)
				; /* do nothing */
			setenv("GIT_AUTHOR_NAME", line, 1);
			setenv("GIT_AUTHOR_EMAIL", email, 1);
			setenv("GIT_AUTHOR_DATE", timestamp, 1);
			free(line);
			return;
		}
		p = eol;
		if (*p == '\n')
			p++;
	}
	die ("No author information found in %s",
			sha1_to_hex(commit->object.sha1));
}

static char *help_msg(const unsigned char *sha1)
{
	static char helpbuf[1024];
	char *msg = getenv("GIT_CHERRY_PICK_HELP");

	if (msg)
		return msg;

	strcpy(helpbuf, "  After resolving the conflicts,\n"
	       "mark the corrected paths with 'git add <paths>' "
	       "or 'git rm <paths>' and commit the result.");

	if (!(flags & PICK_REVERSE)) {
		sprintf(helpbuf + strlen(helpbuf),
			"\nWhen commiting, use the option "
			"'-c %s' to retain authorship and message.",
			find_unique_abbrev(sha1, DEFAULT_ABBREV));
	}
	return helpbuf;
}

static void write_message(struct strbuf *msgbuf, const char *filename)
{
	struct lock_file msg_file;
	int msg_fd;
	msg_fd = hold_lock_file_for_update(&msg_file, filename,
					   LOCK_DIE_ON_ERROR);
	if (write_in_full(msg_fd, msgbuf->buf, msgbuf->len) < 0)
		die_errno("Could not write to %s.", filename);
	strbuf_release(msgbuf);
	if (commit_lock_file(&msg_file) < 0)
		die("Error wrapping up %s", filename);
}

static int revert_or_cherry_pick(int argc, const char **argv)
{
	const char *me;
	struct strbuf msgbuf;
	char *reencoded_message = NULL;
	const char *encoding;
	int failed;

	git_config(git_default_config, NULL);
	me = flags & PICK_REVERSE ? "revert" : "cherry-pick";
	setenv(GIT_REFLOG_ACTION, me, 0);
	parse_args(argc, argv);

	if (read_cache() < 0)
		die("git %s: failed to read the index", me);
	if (!no_commit && index_differs_from("HEAD", 0))
		die ("Dirty index: cannot %s", me);

	if (!commit->buffer)
		return error("Cannot get commit message for %s",
				sha1_to_hex(commit->object.sha1));
	encoding = get_encoding(commit->buffer);
	if (!encoding)
		encoding = "UTF-8";
	if (!git_commit_encoding)
		git_commit_encoding = "UTF-8";
	if ((reencoded_message = reencode_string(commit->buffer,
					git_commit_encoding, encoding)))
		commit->buffer = reencoded_message;

	failed = pick_commit(commit, mainline, flags, &msgbuf);
	if (failed < 0) {
		exit(1);
	} else if (failed > 0) {
		fprintf(stderr, "Automatic %s failed.%s\n",
			me, help_msg(commit->object.sha1));
		write_message(&msgbuf, git_path("MERGE_MSG"));
		rerere();
		exit(1);
	}
	if (!(flags & PICK_REVERSE))
		set_author_ident_env(commit->buffer);
	free(reencoded_message);

	fprintf(stderr, "Finished one %s.\n", me);

	write_message(&msgbuf, git_path("MERGE_MSG"));

	/*
	 * If we are cherry-pick, and if the merge did not result in
	 * hand-editing, we will hit this commit and inherit the original
	 * author date and name.
	 * If we are revert, or if our cherry-pick results in a hand merge,
	 * we had better say that the current user is responsible for that.
	 */

	if (!no_commit) {
		/* 6 is max possible length of our args array including NULL */
		const char *args[6];
		int i = 0;
		args[i++] = "commit";
		args[i++] = "-n";
		if (signoff)
			args[i++] = "-s";
		if (!edit) {
			args[i++] = "-F";
			args[i++] = git_path("MERGE_MSG");
		}
		args[i] = NULL;
		return execv_git_cmd(args);
	}
	return 0;
}

int cmd_revert(int argc, const char **argv, const char *prefix)
{
	if (isatty(0))
		edit = 1;
	flags = PICK_REVERSE | PICK_ADD_NOTE;
	return revert_or_cherry_pick(argc, argv);
}

int cmd_cherry_pick(int argc, const char **argv, const char *prefix)
{
	flags = 0;
	return revert_or_cherry_pick(argc, argv);
}
