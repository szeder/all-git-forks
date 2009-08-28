#include "cache.h"
#include "commit.h"
#include "run-command.h"
#include "cache-tree.h"
#include "pick.h"
#include "merge-recursive.h"

static struct commit *commit;

static char *get_oneline(const char *message)
{
	char *result;
	const char *p = message, *abbrev, *eol;
	int abbrev_len, oneline_len;

	if (!p)
		return NULL;
	while (*p && (*p != '\n' || p[1] != '\n'))
		p++;

	if (*p) {
		p += 2;
		for (eol = p + 1; *eol && *eol != '\n'; eol++)
			; /* do nothing */
	} else
		eol = p;
	abbrev = find_unique_abbrev(commit->object.sha1, DEFAULT_ABBREV);
	abbrev_len = strlen(abbrev);
	oneline_len = eol - p;
	result = xmalloc(abbrev_len + 5 + oneline_len);
	memcpy(result, abbrev, abbrev_len);
	memcpy(result + abbrev_len, "... ", 4);
	memcpy(result + abbrev_len + 4, p, oneline_len);
	result[abbrev_len + 4 + oneline_len] = '\0';
	return result;
}

static void add_message_to_msg(struct strbuf *msg, const char *message)
{
	const char *p = message;
	while (*p && (*p != '\n' || p[1] != '\n'))
		p++;

	if (!*p)
		strbuf_addstr(msg, sha1_to_hex(commit->object.sha1));

	p += 2;
	strbuf_addstr(msg, p);
	return;
}

static struct tree *empty_tree(void)
{
	struct tree *tree = xcalloc(1, sizeof(struct tree));

	tree->object.parsed = 1;
	tree->object.type = OBJ_TREE;
	pretend_sha1_file(NULL, 0, OBJ_TREE, tree->object.sha1);
	return tree;
}

/*
 * Pick changes introduced by "commit" argument into current working
 * tree and index.
 *
 * It starts from the current index (not HEAD), and allow the effect
 * of one commit replayed (either forward or backward) to that state,
 * leaving the result in the index.
 *
 * You do not have to start from a commit, so you can replay many commits
 * to the index in sequence without commiting in between to squash multiple
 * steps if you wanted to.
 *
 * Return 0 on success.
 * Return negative value on error before picking,
 * and a positive value after picking,
 * and return 1 if and only if a conflict occurs but no other error.
 */
int pick_commit(struct commit *pick_commit, int mainline, int flags,
		struct strbuf *msg)
{
	unsigned char head[20];
	struct commit *base, *next, *parent;
	int i, index_fd, clean;
	int ret = 0;
	char *oneline;
	const char *message;
	struct merge_options o;
	struct tree *result, *next_tree, *base_tree, *head_tree;
	static struct lock_file index_lock;

	strbuf_init(msg, 0);
	commit = pick_commit;

	/*
	 * Let's compute the tree that represents the "current" state
	 * for merge-recursive to work on.
	 */
	if (write_cache_as_tree(head, 0, NULL))
		return error("Your index file is unmerged.");
	discard_cache();

	index_fd = hold_locked_index(&index_lock, 0);
	if (index_fd < 0)
		return error("Unable to create locked index: %s",
			     strerror(errno));

	if (!commit->parents) {
		if (flags & PICK_REVERSE)
			return error("Cannot revert a root commit");
		parent = NULL;
	}
	else if (commit->parents->next) {
		/* Reverting or cherry-picking a merge commit */
		int cnt;
		struct commit_list *p;

		if (!mainline)
			return error("Commit %s is a merge but no mainline was given.",
				     sha1_to_hex(commit->object.sha1));

		for (cnt = 1, p = commit->parents;
		     cnt != mainline && p;
		     cnt++)
			p = p->next;
		if (cnt != mainline || !p)
			return error("Commit %s does not have parent %d",
				     sha1_to_hex(commit->object.sha1),
				     mainline);
		parent = p->item;
	} else if (0 < mainline)
		return error("Mainline was specified but commit %s is not a merge.",
			     sha1_to_hex(commit->object.sha1));
	else
		parent = commit->parents->item;

	if (!(message = commit->buffer))
		return error("Cannot get commit message for %s",
			     sha1_to_hex(commit->object.sha1));

	if (parent && parse_commit(parent) < 0)
		return error("Cannot parse parent commit %s",
			     sha1_to_hex(parent->object.sha1));

	oneline = get_oneline(message);

	if (flags & PICK_REVERSE) {
		char *oneline_body = strchr(oneline, ' ');

		base = commit;
		next = parent;
		strbuf_addstr(msg, "Revert \"");
		strbuf_addstr(msg, oneline_body + 1);
		strbuf_addstr(msg, "\"\n\nThis reverts commit ");
		strbuf_addstr(msg, sha1_to_hex(commit->object.sha1));

		if (commit->parents->next) {
			strbuf_addstr(msg, ", reversing\nchanges made to ");
			strbuf_addstr(msg, sha1_to_hex(parent->object.sha1));
		}
		strbuf_addstr(msg, ".\n");
	} else {
		base = parent;
		next = commit;
		add_message_to_msg(msg, message);
		if (flags & PICK_ADD_NOTE) {
			strbuf_addstr(msg, "(cherry picked from commit ");
			strbuf_addstr(msg, sha1_to_hex(commit->object.sha1));
			strbuf_addstr(msg, ")\n");
		}
	}

	read_cache();
	init_merge_options(&o);
	o.branch1 = "HEAD";
	o.branch2 = oneline;

	head_tree = parse_tree_indirect(head);
	next_tree = next ? next->tree : empty_tree();
	base_tree = base ? base->tree : empty_tree();

	clean = merge_trees(&o,
			    head_tree,
			    next_tree, base_tree, &result);

	if (active_cache_changed &&
	    (write_cache(index_fd, active_cache, active_nr) ||
	     commit_locked_index(&index_lock))) {
		error("Unable to write new index file");
		return 2;
	}
	rollback_lock_file(&index_lock);

	if (!clean) {
		strbuf_addstr(msg, "\nConflicts:\n\n");
		for (i = 0; i < active_nr;) {
			struct cache_entry *ce = active_cache[i++];
			if (ce_stage(ce)) {
				strbuf_addstr(msg, "\t");
				strbuf_addstr(msg, ce->name);
				strbuf_addstr(msg, "\n");
				while (i < active_nr && !strcmp(ce->name,
						active_cache[i]->name))
					i++;
			}
		}
		ret = 1;
	}
	free(oneline);

	discard_cache();
	if (read_cache() < 0) {
		error("Cannot read the index");
		return 2;
	}

	return ret;
}
