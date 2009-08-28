#include "builtin.h"
#include "cache.h"
#include "parse-options.h"
#include "run-command.h"
#include "refs.h"
#include "diff.h"
#include "unpack-trees.h"

#define SEQ_DIR "rebase-merge"

#define PATCH_FILE	git_path(SEQ_DIR "/patch")

static char *reflog;

static int allow_dirty = 0, verbosity = 1, advice = 1;

static unsigned char head_sha1[20];

static const char * const git_sequencer_helper_usage[] = {
	"git sequencer--helper --make-patch <commit>",
	"git sequencer--helper --reset-hard <commit> <reflog-msg> <verbosity>",
	"git sequencer--helper --fast-forward <commit> <reflog-msg> <verbosity>",
	NULL
};

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
	struct option options[] = {
		OPT_STRING(0, "make-patch", &patch_commit, "commit",
			   "create a patch from commit"),
		OPT_STRING(0, "reset-hard", &reset_commit, "commit",
			   "reset to commit"),
		OPT_STRING(0, "fast-forward", &ff_commit, "commit",
			   "fast forward to commit"),
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

	if (ff_commit || reset_commit) {
		unsigned char sha1[20];
		char *commit = ff_commit ? ff_commit : reset_commit;

		if (argc != 2)
			usage_with_options(git_sequencer_helper_usage,
					   options);

		if (get_sha1(commit, sha1)) {
			error("Could not find '%s'", commit);
			return 1;
		}

		reflog = (char *)argv[0];

		if (parse_verbosity(argv[1])) {
			error("bad verbosity '%s'", argv[1]);
			return 1;
		}

		if (ff_commit)
			return do_fast_forward(sha1);
		else
			return reset_almost_hard(sha1);
	}

	usage_with_options(git_sequencer_helper_usage, options);
}
