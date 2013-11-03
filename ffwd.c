#include "git-compat-util.h"
#include "strbuf.h"
#include "string-list.h"
#include "remote.h"
#include "refs.h"
#include "cache.h"
#include "branch.h"

// Taken from builtin/remote.c, but maybe should live elsewhere?
static const char *abbrev_ref(const char *name, const char *prefix)
{
	const char *abbrev = skip_prefix(name, prefix);
	if (abbrev)
		return abbrev;
	return name;
}
#define abbrev_branch(name) abbrev_ref((name), "refs/heads/")

struct tracking_branch_collector
{
	struct remote *remote;
	struct string_list *branches;
	int exclude_cherry_picks;
};

static int append_local_tracking_branch_to_list(const char *refname,
	const unsigned char *sha1, int flags, void *cb_data)
{
	struct tracking_branch_collector *tbc = cb_data;
	int i;
	unsigned char src_sha1[20];
	unsigned char dst_sha1[20];

	// ### Can probably pull branch sha1 from here instead.
	(void) sha1;

	if (flags & REF_ISSYMREF)
		return 0;

	struct branch *branch = branch_get(refname);

	if (!branch->remote_name || strcmp(branch->remote_name,
					   tbc->remote->name))
		return 0;

	for (i = 0; i < branch->merge_nr; i++)
	{

		if (get_sha1(branch->merge[i]->src, src_sha1))
			return -1;

		/* The upstream branch may not actually exist as the result of
		   bad configuration.  Skip this branch in that case. */
		if (get_sha1(branch->merge[i]->dst, dst_sha1))
			continue;

		if (!memcmp(src_sha1, dst_sha1, sizeof(src_sha1)))
			continue;

		if (ref_newer(dst_sha1, src_sha1)) {
			struct string_list_item *item = string_list_append(
				tbc->branches, branch->name);
			item->util = branch;
		}
		else {
			int num_ours, num_theirs;
			int ret = stat_tracking_info(branch,
						     tbc->exclude_cherry_picks,
						     &num_ours, &num_theirs);
			if (ret > 0) {
				printf_ln("branch %s is %d commits behind and %d commits ahead of %s.  cannot be fast-forwarded.",
					  abbrev_ref(branch->name, "refs/heads/"),
					  num_theirs, num_ours,
					  abbrev_ref(branch->merge[i]->dst, "refs/remotes/"));
			}
		}
	}

	return 0;
}

static int get_local_tracking_branches(struct remote *remote,
				       struct string_list *branches,
				       int exclude_cherry_picks)
{
	struct tracking_branch_collector tbc = {remote, branches,
		exclude_cherry_picks};

	return for_each_branch_ref(append_local_tracking_branch_to_list, &tbc);
}

int ffwd(struct string_list *remotes)
{
	struct string_list_item *r;
	struct string_list_item *b;
	struct string_list branches = STRING_LIST_INIT_DUP;
	struct branch *current_branch = branch_get(NULL);
	const struct ref_update **updates = NULL;
	int update_alloc = 0;
	int update_count = 0;
	int update_current_branch = 0;
	int result = 0;

	for_each_string_list_item(r, remotes) {
		struct remote *remote = remote_get(r->string);

		result = get_local_tracking_branches(remote, &branches, 1);
		if (result != 0)
			break;

		/* if (!ref_exists(branch->refname)) */
		/* 	die(_("branch '%s' does not exist"), branch->name); */
		/* update_refs(); */

		for_each_string_list_item(b, &branches) {
			struct branch *branch = b->util;

			// ### This is here for debugging.  Remove when done.
			unsigned char src_sha1[20];
			unsigned char dst_sha1[20];

			get_sha1(branch->merge[0]->src, src_sha1);
			get_sha1(branch->merge[0]->dst, dst_sha1);

			printf_ln("    %s (%s) <= %s (%s)",
				  abbrev_ref(branch->merge_name[0], "refs/heads/"),
				  sha1_to_hex(src_sha1),
				  abbrev_ref(branch->merge[0]->dst, "refs/remotes/"),
				  sha1_to_hex(dst_sha1));

			if (branch != current_branch) {
				struct ref_update *update;

				ALLOC_GROW(updates, update_count + 1, update_alloc);
				update = xcalloc(1, sizeof(*update));
				updates[update_count++] = update;

				memcpy(update->new_sha1, dst_sha1, sizeof(dst_sha1));
				memcpy(update->old_sha1, src_sha1, sizeof(src_sha1));
				update->have_old = 1;

				update->ref_name = branch->merge[0]->src;
			}
			else
				update_current_branch = 1;
		}

		string_list_clear(&branches, 0);
	}

	/* Update the refs... */
	printf_ln("%d refs to be updated", update_count + update_current_branch);
	printf_ln("Current branch %s be updated",
		  update_current_branch ? "will be" : "will not");

	update_refs("ffwd: updated to match remote tracking branch",
		    updates, update_count, DIE_ON_ERR);

	return result;
}
