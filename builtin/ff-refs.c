#include "cache.h"
#include "refs.h"
#include "builtin.h"
#include "remote.h"
#include "run-command.h"
#include "worktree.h"

struct worktree **worktrees;
const char *padding = ".....................................................";

static const char * const builtin_ff_refs_usage[] = {
	N_("git ff-refs [<options>]"),
	NULL
};

enum ff_result_type {
	UP_TO_DATE,
	UPDATABLE,
	REMOTE_MISSING,
	NON_FAST_FORWARD,
	UNABLE_TO_UPDATE
};

struct ff_ref_details {
	struct branch *branch;
	const char *upstream;
	const char *shortened_upstream;
	int names_length;
	enum ff_result_type result_type;

	struct commit *branch_commit;
	struct commit *upstream_commit;
	struct commit *merge_base;
	struct worktree *wt;
};

struct ff_ref_data {
	int max_names_length;

	int detail_counter;
	int detail_alloc;
	struct ff_ref_details **detail_list;
};

static const char *result_type_str(enum ff_result_type result_type)
{
	switch (result_type) {
	case UP_TO_DATE:
		return _("UP-TO-DATE");
	case UPDATABLE:
		return _("WOULD-UPDATE");
	case REMOTE_MISSING:
		return _("REMOTE-MISSING");
	case NON_FAST_FORWARD:
		return _("NON-FAST-FORWARD");
	default:
		return _("UNABLE-TO-UPDATE");
	}
}

/**
 * Do the ref update.
 *  - If the ref is checked out in any worktree, emulate merge --ff-only to
 *    also update the local work tree (including executing the post-merge hook).
 *
 *  - If the ref is not checked out in any worktree, update it
 *
 *  - If any of the ref updates fails, the result_type is set to UNABLE-TO-UPDATE
 */
static void do_ref_update(struct ff_ref_data *data, struct ff_ref_details *details)
{
	const char *refname = details->branch->refname;

	if (details->wt) {
		struct strbuf path = STRBUF_INIT;

		strbuf_getcwd(&path);
		chdir(details->wt->path);
		set_git_dir(details->wt->git_dir);
		read_index(&the_index);

		if (checkout_fast_forward(details->branch_commit->object.sha1,
				details->upstream_commit->object.sha1, 1))
			details->result_type = NON_FAST_FORWARD;
		else if (update_ref("ff-refs", refname, details->upstream_commit->object.sha1,
				details->branch_commit->object.sha1, 0, UPDATE_REFS_QUIET_ON_ERR)) {
			details->result_type = UNABLE_TO_UPDATE;
			run_hook_le(NULL, "post-merge", "0", NULL);
		}
		discard_index(&the_index);
		chdir(path.buf);
		strbuf_release(&path);
	} else if (update_ref("ff-refs", refname, details->upstream_commit->object.sha1,
			details->branch_commit->object.sha1, 0, UPDATE_REFS_QUIET_ON_ERR))
		details->result_type = UNABLE_TO_UPDATE;
}

/**
 * return the worktree with the given refname checked out, or NULL if that
 * ref is not checked out in any branch.
 *
 * This implementation assumes a small number of worktrees (since it loops
 * through each worktree for every ref).  If a repository has a large number
 * of worktrees, then it might be beneficial to implement this as a hashmap
 * lookup instead.
 */
static struct worktree *find_worktree(const char *refname)
{
	int i = 0;

	for (i = 0; worktrees[i]; i++) {
		if (!worktrees[i]->is_detached && !strcmp(worktrees[i]->head_ref, refname)) {
			return worktrees[i];
		}
	}
	return NULL;
}

/**
 * After all of the relevant refs have been collected, process the
 * interesting ones
 */
static void process_refs(struct ff_ref_data *data)
{
	int i = 0;

	for (i = 0; data->detail_list[i]; i++) {
		struct ff_ref_details *details;
		int padLen;

		details = data->detail_list[i];
		padLen = 3 + data->max_names_length - details->names_length;
		if (padLen < 0)
			padLen = 0;

		printf("     %s -> %s%*.*s",
			details->branch->name, details->shortened_upstream, padLen, padLen, padding);
		if (details->result_type == UPDATABLE)
			do_ref_update(data, details);

		printf("[%s]\n", result_type_str(details->result_type));
	}
}

static void add_to_detail_list(struct ff_ref_data *data,
		struct ff_ref_details *details)
{
	if (!data->detail_alloc) {
		data->detail_list = xmalloc(sizeof(struct ff_ref_details *));
		data->detail_alloc = 1;
	} else
		ALLOC_GROW(data->detail_list, data->detail_counter + 1, data->detail_alloc);

	if (details && details->names_length > data->max_names_length)
		data->max_names_length = details->names_length;

	data->detail_list[data->detail_counter++] = details;
}

/**
 * Look for refs which have an upstream configured.  Each ref with an upstream
 * is added to a list to later possibly make changes on.  All of the necessary
 * read-only data is gleaned here.
 */
static int analize_refs(const char *refname,
			const struct object_id *oid, int flags, void *cb_data) {

	struct branch *branch;
	const char *upstream;
	struct ff_ref_data *data = cb_data;

	branch = branch_get(shorten_unambiguous_ref(refname, 0));
	upstream = branch_get_upstream(branch, NULL);
	if (upstream) {
		struct ff_ref_details *details = xmalloc(sizeof(struct ff_ref_details));
		unsigned char upstream_hash[GIT_SHA1_RAWSZ];

		details->branch = branch;
		details->upstream = upstream;

		details->shortened_upstream = shorten_unambiguous_ref(upstream, 0);
		details->branch_commit = NULL;
		details->upstream_commit = NULL;
		details->merge_base = NULL;
		details->result_type = UNABLE_TO_UPDATE;
		details->names_length = strlen(branch->name) +
				strlen(details->shortened_upstream);
		details->wt = find_worktree(details->branch->refname);

		if (!resolve_ref_unsafe(details->upstream, RESOLVE_REF_READING,
				upstream_hash, NULL))
			details->result_type = REMOTE_MISSING;

		else if (!hashcmp(oid->hash, upstream_hash))
			details->result_type = UP_TO_DATE;
		else {
			struct commit_list *bases;

			details->branch_commit = lookup_commit_reference(oid->hash);
			details->upstream_commit = lookup_commit_reference(upstream_hash);
			bases = get_merge_bases(details->branch_commit,
					details->upstream_commit);
			details->merge_base = bases->item;

			if (!hashcmp(upstream_hash, details->merge_base->object.sha1))
				details->result_type = UP_TO_DATE;

			else if (!in_merge_bases(details->branch_commit, details->upstream_commit))
				details->result_type = NON_FAST_FORWARD;

			else
				details->result_type = UPDATABLE;
		}
		add_to_detail_list(data, details);
	}
	return 0;
}

/**
 * Free the memory allocated for all of the data
 */
static void free_data(struct ff_ref_data *data)
{
	int i = 0;

	for (i = 0; data->detail_list[i]; i++)
		free(data->detail_list[i]);
	free(data);
}

int cmd_ff_refs(int argc, const char **argv, const char *prefix)
{
	int ret = 0;

	struct option options[] = {
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options, builtin_ff_refs_usage, 0);
	if (argc)
		usage_with_options(builtin_ff_refs_usage, options);
	else {
		struct ff_ref_data *data = NULL;

		worktrees = get_worktrees();
		data = xmalloc(sizeof(struct ff_ref_data));
		data->detail_alloc = 0;
		data->detail_counter = 0;
		data->max_names_length = 0;

		ret = for_each_ref(&analize_refs, data);
		add_to_detail_list(data, NULL);

		//for each detail
		process_refs(data);

		free_worktrees(worktrees);
		free_data(data);
	}
	return ret;
}
