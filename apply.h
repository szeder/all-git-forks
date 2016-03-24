#ifndef APPLY_H
#define APPLY_H

enum ws_error_action {
	nowarn_ws_error,
	warn_on_ws_error,
	die_on_ws_error,
	correct_ws_error
};


enum ws_ignore {
	ignore_ws_none,
	ignore_ws_change
};

/*
 * We need to keep track of how symlinks in the preimage are
 * manipulated by the patches.  A patch to add a/b/c where a/b
 * is a symlink should not be allowed to affect the directory
 * the symlink points at, but if the same patch removes a/b,
 * it is perfectly fine, as the patch removes a/b to make room
 * to create a directory a/b so that a/b/c can be created.
 *
 * See also "struct string_list symlink_changes" in "struct
 * apply_state".
 */
#define SYMLINK_GOES_AWAY 01
#define SYMLINK_IN_RESULT 02

struct apply_state {
	const char *prefix;
	int prefix_length;

	/*
	 * Since lockfile.c keeps a linked list of all created
	 * lock_file structures, it isn't safe to free(lock_file).
	 */
	struct lock_file *lock_file;

	int apply;
	int allow_overlap;
	int apply_in_reverse;
	int apply_with_reject;
	int apply_verbosely;

	/* --cached updates only the cache without ever touching the working tree. */
	int cached;

	/* --stat does just a diffstat, and doesn't actually apply */
	int diffstat;

	/* --numstat does numeric diffstat, and doesn't actually apply */
	int numstat;

	const char *fake_ancestor;

	int summary;

	int threeway;

	int no_add;

	/*
	 *  --check turns on checking that the working tree matches the
	 *    files that are being modified, but doesn't apply the patch
	 */
	int check;

	/* --index updates the cache as well. */
	int check_index;

	int unidiff_zero;

	int update_index;

	int unsafe_paths;

	int line_termination;

	/*
	 * For "diff-stat" like behaviour, we keep track of the biggest change
	 * we've seen, and the longest filename. That allows us to do simple
	 * scaling.
	 */
	int max_change;
	int max_len;

	/*
	 * Various "current state", notably line numbers and what
	 * file (and how) we're patching right now.. The "is_xxxx"
	 * things are flags, where -1 means "don't know yet".
	 */
	int linenr;

	/*
	 * Records filenames that have been touched, in order to handle
	 * the case where more than one patches touch the same file.
	 */
	struct string_list fn_table;

	struct string_list symlink_changes;

	int p_value;
	int p_value_known;
	unsigned int p_context;

	const char *patch_input_file;

	struct string_list limit_by_name;
	int has_include;

	struct strbuf root;

	const char *whitespace_option;
	int whitespace_error;
	int squelch_whitespace_errors;
	int applied_after_fixing_ws;

	enum ws_error_action ws_error_action;
	enum ws_ignore ws_ignore_action;
};

extern int parse_whitespace_option(struct apply_state *state,
				   const char *option);
extern int parse_ignorewhitespace_option(struct apply_state *state,
					 const char *option);

extern int init_apply_state(struct apply_state *state, const char *prefix);
extern int check_apply_state(struct apply_state *state, int force_apply);

#define APPLY_OPT_INACCURATE_EOF	(1<<0)
#define APPLY_OPT_RECOUNT		(1<<1)

extern int apply_all_patches(struct apply_state *state,
			     int argc,
			     const char **argv,
			     int options);

#endif
