#include "builtin.h"
#include "cache.h"
#include "parse-options.h"
#include "run-command.h"
#include "refs.h"
#include "diff.h"
#include "unpack-trees.h"
#include "string-list.h"
#include "pick.h"
#include "rerere.h"
#include "dir.h"
#include "cache-tree.h"
#include "utf8.h"

#define SEQ_DIR "rebase-merge"

#define PATCH_FILE	git_path(SEQ_DIR "/patch")
#define MERGE_MSG	git_path("MERGE_MSG")
#define SQUASH_MSG	git_path("SQUASH_MSG")

/**********************************************************************
 * Data structures
 */

struct user_info {
	const char *name;
	const char *mail;
	const char *time; /* "<timestamp> <timezone>" */
};

struct commit_info {
	struct user_info author; /* author info */
	struct user_info committer; /* not used, but for easy extendability */
	const char *encoding; /* encoding */
	char *subject; /* basically the first line of the summary */
	struct strbuf summary; /* the commit message */
	char *source; /* source of the commit message, either
		       * "message", "merge", "squash" or a commit SHA1 */
	char *patch; /* a patch */
	struct string_list parents; /* list of parents' hex'ed sha1 ids */
};

/**********************************************************************
 * Global variables
 */

static char *reflog;

static int squash_count = 0;

static int allow_dirty = 0, verbosity = 1, advice = 1;

static unsigned char head_sha1[20];

static struct commit_info next_commit;

static const char * const git_sequencer_helper_usage[] = {
	"git sequencer--helper --make-patch <commit>",
	"git sequencer--helper --reset-hard <commit> <reflog-msg> "
		"<verbosity> [<allow-dirty>]",
	"git sequencer--helper --fast-forward <commit> <reflog-msg> "
		"<verbosity> [<allow-dirty>]",
	"git sequencer--helper --cherry-pick <commit> <reflog-msg> "
		"<verbosity> [<do-not-commit>]",
	NULL
};

/**********************************************************************
 * Sequencer functions
 */

static int parse_and_init_tree_desc(const unsigned char *sha1,
				    struct tree_desc *desc)
{
	struct tree *tree = parse_tree_indirect(sha1);
	if (!tree)
		return 1;
	init_tree_desc(desc, tree->buffer, tree->size);
	return 0;
}

static int reset_index_file(const unsigned char *sha1, int update, int dirty)
{
	int nr = 1;
	int newfd;
	struct tree_desc desc[2];
	struct unpack_trees_options opts;
	struct lock_file *lock = xcalloc(1, sizeof(struct lock_file));

	memset(&opts, 0, sizeof(opts));
	opts.head_idx = 1;
	opts.src_index = &the_index;
	opts.dst_index = &the_index;
	opts.reset = 1; /* ignore unmerged entries and overwrite wt files */
	opts.merge = 1;
	opts.fn = oneway_merge;
	if (verbosity > 2)
		opts.verbose_update = 1;
	if (update) /* update working tree */
		opts.update = 1;

	newfd = hold_locked_index(lock, 1);

	read_cache_unmerged();

	if (dirty) {
		if (get_sha1("HEAD", head_sha1))
			return error("You do not have a valid HEAD.");
		if (parse_and_init_tree_desc(head_sha1, desc))
			return error("Failed to find tree of HEAD.");
		nr++;
		opts.fn = twoway_merge;
	}

	if (parse_and_init_tree_desc(sha1, desc + nr - 1))
		return error("Failed to find tree of %s.", sha1_to_hex(sha1));
	if (unpack_trees(nr, desc, &opts))
		return -1;
	if (write_cache(newfd, active_cache, active_nr) ||
	    commit_locked_index(lock))
		return error("Could not write new index file.");

	return 0;
}

/*
 * Realize reset --hard behavior.
 * If allow_dirty is set and there is a dirty work tree,
 * then the changes in the work tree are to be kept.
 *
 * This should be faster than calling "git reset --hard" because
 * this calls "unpack_trees()" directly (instead of forking and
 * execing "git read-tree").
 *
 * Unmerged entries in the index will be discarded.
 *
 * If allow_dirty is set and fast forwarding the work tree
 * fails because it is dirty, then the work tree will not be
 * updated.
 *
 * No need to read or discard the index before calling this
 * function.
 */
static int reset_almost_hard(const unsigned char *sha)
{
	int err = allow_dirty ?
		(reset_index_file(sha, 1, 1) || reset_index_file(sha, 0, 0)) :
		reset_index_file(sha, 1, 0);
	if (err)
		return error("Could not reset index.");

	return update_ref(reflog, "HEAD", sha, NULL, 0, MSG_ON_ERR);
}

/* Generate purely informational patch file */
static void make_patch(struct commit *commit)
{
	struct commit_list *parents = commit->parents;
	const char **args;
	struct child_process chld;
	int i;
	int fd = open(PATCH_FILE, O_WRONLY | O_CREAT, 0666);
	if (fd < 0)
		return;

	memset(&chld, 0, sizeof(chld));
	if (!parents) {
		write(fd, "Root commit\n", 12);
		close(fd);
		return;
	} else if (!parents->next) {
		args = xcalloc(5, sizeof(char *));
		args[0] = "diff-tree";
		args[1] = "-p";
		args[2] = xstrdup(sha1_to_hex(parents->item->object.sha1));
		args[3] = xstrdup(sha1_to_hex(((struct object *)commit)->sha1));
	} else {
		int count = 1;

		for (; parents; parents = parents->next)
			++count;

		i = 0;
		args = xcalloc(count + 3, sizeof(char *));
		args[i++] = "diff";
		args[i++] = "--cc";
		args[i++] = xstrdup(sha1_to_hex(commit->object.sha1));

		for (parents = commit->parents; parents;
		     parents = parents->next) {
			char *hex = sha1_to_hex(parents->item->object.sha1);
			args[i++] = xstrdup(hex);
		}
	}

	chld.argv = args;
	chld.git_cmd = 1;
	chld.out = fd;

	/* Run, ignore errors. */
	if (!start_command(&chld))
		finish_command(&chld);

	for (i = 2; args[i]; i++)
		free((char *)args[i]);
	free(args);
}

/* Commit current index with information next_commit onto parent_sha1. */
static int do_commit(unsigned char *parent_sha1)
{
	int failed;
	unsigned char tree_sha1[20];
	unsigned char commit_sha1[20];
	struct strbuf sbuf;
	const char *reencoded = NULL;

	if (squash_count) {
		squash_count = 0;
		if (file_exists(SQUASH_MSG))
			unlink(SQUASH_MSG);
	}

	if (!index_differs_from("HEAD", 0) &&
	    !next_commit.parents.nr)
		return error("No changes! Do you really want an empty commit?");

	if (!next_commit.author.name || !next_commit.author.mail)
		return error("Internal error: Author information not set properly.");

	if (write_cache_as_tree(tree_sha1, 0, NULL))
		return 1;

	if (!next_commit.encoding)
		next_commit.encoding = xstrdup("utf-8");
	if (!git_commit_encoding)
		git_commit_encoding = "utf-8";

	strbuf_init(&sbuf, 8192); /* should avoid reallocs for the headers */
	strbuf_addf(&sbuf, "tree %s\n", sha1_to_hex(tree_sha1));
	if (parent_sha1)
		strbuf_addf(&sbuf, "parent %s\n", sha1_to_hex(parent_sha1));
	if (next_commit.parents.nr) {
		int i;
		for (i = 0; i < next_commit.parents.nr; ++i)
			strbuf_addf(&sbuf, "parent %s\n",
					next_commit.parents.items[i].string);
	}
	if (!next_commit.author.time) {
		char time[50];
		datestamp(time, sizeof(time));
		next_commit.author.time = xstrdup(time);
	}

	stripspace(&next_commit.summary, 1);

	/* if encodings differ, reencode whole buffer */
	if (strcasecmp(git_commit_encoding, next_commit.encoding)) {
		if ((reencoded = reencode_string(next_commit.author.name,
				git_commit_encoding, next_commit.encoding))) {
			free((void *)next_commit.author.name);
			next_commit.author.name = reencoded;
		}
		if ((reencoded = reencode_string(next_commit.summary.buf,
				git_commit_encoding, next_commit.encoding))) {
			strbuf_reset(&next_commit.summary);
			strbuf_addstr(&next_commit.summary, reencoded);
		}
	}
	strbuf_addf(&sbuf, "author %s <%s> %s\n", next_commit.author.name,
			next_commit.author.mail, next_commit.author.time);
	strbuf_addf(&sbuf, "committer %s\n", git_committer_info(0));
	if (!is_encoding_utf8(git_commit_encoding))
		strbuf_addf(&sbuf, "encoding %s\n", git_commit_encoding);
	strbuf_addch(&sbuf, '\n');
	strbuf_addbuf(&sbuf, &next_commit.summary);
	if (sbuf.buf[sbuf.len-1] != '\n')
		strbuf_addch(&sbuf, '\n');

	failed = write_sha1_file(sbuf.buf, sbuf.len, commit_type, commit_sha1);
	strbuf_release(&sbuf);
	if (failed)
		return 1;

	if (verbosity > 1)
		printf("Created %scommit %s\n",
			parent_sha1 || next_commit.parents.nr ? "" : "initial ",
			sha1_to_hex(commit_sha1));

	if (update_ref(reflog, "HEAD", commit_sha1, NULL, 0, 0))
		return error("Could not update HEAD to %s.",
			     sha1_to_hex(commit_sha1));

	return 0;
}

/*
 * Fill next_commit.author according to ident.
 * Ident may have one of the following forms:
 * 	"name <e-mail> timestamp timezone\n..."
 * 	"name <e-mail> timestamp timezone"
 * 	"name <e-mail>"
 */
static void set_author_info(const char *ident)
{
	const char *tmp1 = strstr(ident, " <");
	const char *tmp2;
	char *data;
	if (!tmp1)
		return;
	tmp2 = strstr(tmp1+2, ">");
	if (!tmp2)
		return;
	if (tmp2[1] != 0 && tmp2[1] != ' ')
		return;

	data = xmalloc(strlen(ident)); /* a trivial upper bound */

	snprintf(data, tmp1-ident+1, "%s", ident);
	next_commit.author.name = xstrdup(data);
	snprintf(data, tmp2-tmp1-1, "%s", tmp1+2);
	next_commit.author.mail = xstrdup(data);

	if (tmp2[1] == 0) {
		free(data);
		return;
	}

	tmp1 = strpbrk(tmp2+2, "\r\n");
	if (!tmp1)
		tmp1 = tmp2 + strlen(tmp2);

	snprintf(data, tmp1-tmp2-1, "%s", tmp2+2);
	next_commit.author.time = xstrdup(data);
	free(data);
}

static void set_message_source(const char *source)
{
	if (next_commit.source)
		free(next_commit.source);
	next_commit.source = xstrdup(source);
}

/* Set subject, an information for the case of conflict */
static void set_pick_subject(const char *hex, struct commit *commit)
{
	const char *tmp = strstr(commit->buffer, "\n\n");
	if (tmp) {
		const char *eol;
		int len = strlen(hex);
		tmp += 2;
		eol = strchrnul(tmp, '\n');
		next_commit.subject = xmalloc(eol - tmp + len + 5);
		snprintf(next_commit.subject, eol - tmp + len + 5, "%s... %s",
								hex, tmp);
	}
}

/* Return a commit object of "arg" */
static struct commit *get_commit(const char *arg)
{
	unsigned char sha1[20];

	if (get_sha1(arg, sha1)) {
		error("Could not find '%s'", arg);
		return NULL;
	}
	return lookup_commit_reference(sha1);
}

static int do_fast_forward(const unsigned char *sha)
{
	if (reset_almost_hard(sha))
		return error("Cannot fast forward to %s", sha1_to_hex(sha));
	if (verbosity > 1)
		printf("Fast forward to %s\n", sha1_to_hex(sha));
	return 0;
}

static int set_verbosity(int verbose)
{
	char tmp[] = "0";
	verbosity = verbose;
	if (verbosity <= 0) {
		verbosity = 0;
		advice = 0;
	} else if (verbosity > 5)
		verbosity = 5;
	/* Git does not run on EBCDIC, so we rely on ASCII: */
	tmp[0] += verbosity;
	setenv("GIT_MERGE_VERBOSITY", tmp, 1);
	return 0;
}

static int write_commit_summary_into(const char *filename)
{
	struct lock_file *lock = xcalloc(1, sizeof(struct lock_file));
	int fd = hold_lock_file_for_update(lock, filename, 0);
	if (fd < 0)
		return error("Unable to create '%s.lock': %s", filename,
							strerror(errno));
	if (write_in_full(fd, next_commit.summary.buf,
			      next_commit.summary.len) < 0)
		return error("Could not write to %s: %s",
						filename, strerror(errno));
	if (commit_lock_file(lock) < 0)
		return error("Error wrapping up %s", filename);
	return 0;
}

static int do_cherry_pick(char *cp_commit, int no_commit)
{
	struct commit *commit;
	int failed;
	const char *author;

	if (get_sha1("HEAD", head_sha1))
		return error("You do not have a valid HEAD.");

	commit = get_commit(cp_commit);
	if (!commit)
		return 1;

	set_pick_subject(cp_commit, commit);

	failed = pick_commit(commit, 0, 0, &next_commit.summary);

	set_message_source(sha1_to_hex(commit->object.sha1));
	author = strstr(commit->buffer, "\nauthor ");
	if (author)
		set_author_info(author + 8);

	/* We do not want extra Conflicts: lines on cherry-pick,
	   so just take the old commit message. */
	if (failed) {
		strbuf_setlen(&next_commit.summary, 0);
		strbuf_addstr(&next_commit.summary,
			      strstr(commit->buffer, "\n\n") + 2);
		rerere();
		make_patch(commit);
		write_commit_summary_into(MERGE_MSG);
		return error(pick_help_msg(commit->object.sha1, 0));
	}

	if (!no_commit && do_commit(head_sha1))
		return error("Could not commit.");

	return 0;
}

/**********************************************************************
 * Builtin sequencer helper functions
 */

/* v should be "" or "t" or "\d" */
static int parse_verbosity(const char *v)
{
	/* "" means verbosity = 1 */
	if (!v[0])
		return set_verbosity(1);

	if (v[1])
		return 1;

	if (v[0] == 't')
		return set_verbosity(2);

	if (!isdigit(v[0]))
		return 1;

	return set_verbosity(v[0] - '0');
}

int cmd_sequencer__helper(int argc, const char **argv, const char *prefix)
{
	char *patch_commit = NULL;
	char *reset_commit = NULL;
	char *ff_commit = NULL;
	char *cp_commit = NULL;
	struct option options[] = {
		OPT_STRING(0, "make-patch", &patch_commit, "commit",
			   "create a patch from commit"),
		OPT_STRING(0, "reset-hard", &reset_commit, "commit",
			   "reset to commit"),
		OPT_STRING(0, "fast-forward", &ff_commit, "commit",
			   "fast forward to commit"),
		OPT_STRING(0, "cherry-pick", &cp_commit, "commit",
			   "cherry pick commit"),
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options,
			     git_sequencer_helper_usage, 0);

	if (patch_commit) {
		struct commit *c = get_commit(patch_commit);
		if (!c)
			return 1;

		make_patch(c);

		return 0;
	}

	if (cp_commit || ff_commit || reset_commit) {
		unsigned char sha1[20];
		char *commit;
		int opt_arg = 0;

		if (argc != 2 && argc != 3)
			usage_with_options(git_sequencer_helper_usage,
					   options);

		reflog = (char *)argv[0];

		if (parse_verbosity(argv[1])) {
			error("bad verbosity '%s'", argv[1]);
			return 1;
		}

		if (argc == 3 && *argv[2] && strcmp(argv[2], "0"))
			opt_arg = 1;

		if (cp_commit)
			return do_cherry_pick(cp_commit, opt_arg);

		allow_dirty = opt_arg;

		commit = ff_commit ? ff_commit : reset_commit;
		if (get_sha1(commit, sha1)) {
			error("Could not find '%s'", commit);
			return 1;
		}

		if (ff_commit)
			return do_fast_forward(sha1);
		else
			return reset_almost_hard(sha1);
	}

	usage_with_options(git_sequencer_helper_usage, options);
}
