#include "cache.h"
#include "builtin.h"
#include "revision.h"
#include "sha1-array.h"

#include <stdio.h>

static int verbose;

static int tree_contains_any(struct tree *tree, struct sha1_array *to_find,
	struct strbuf *path)
{
	struct tree_desc desc;
	struct name_entry entry;

	if (parse_tree(tree)) {
		return -1;
	}

	init_tree_desc(&desc, tree->buffer, tree->size);

	while (tree_entry(&desc, &entry)) {
		if (sha1_array_lookup(to_find, entry.sha1) >= 0) {
			if (path)
				strbuf_addstr(path, entry.path);
			return 1;
		}
		if (S_ISDIR(entry.mode)) {
			if (tree_contains_any(lookup_tree(entry.sha1), to_find, path)) {
				size_t n = strlen(entry.path);
				if (path) {
					strbuf_insert(path, 0, entry.path, n + 1);
					path->buf[n] = '/';
				}
				return 1;
			}
		}
	}
	return 0;
}

static void find_all_contained_in_tree(struct tree *tree, struct sha1_array *to_find,
	struct sha1_array *found)
{
	struct tree_desc desc;
	struct name_entry entry;

	if (parse_tree(tree))
		return;

	init_tree_desc(&desc, tree->buffer, tree->size);

	while (tree_entry(&desc, &entry)) {
		if (sha1_array_lookup(to_find, entry.sha1) >= 0)
			sha1_array_append(found, entry.sha1);

		if (S_ISDIR(entry.mode))
			find_all_contained_in_tree(lookup_tree(entry.sha1),
					to_find, found);
	}
}

static void remove_found_in_tree(struct tree *tree, struct sha1_array *found)
{
	struct tree_desc desc;
	struct name_entry entry;
	int index;

	if (parse_tree(tree))
		return;

	init_tree_desc(&desc, tree->buffer, tree->size);

	while (tree_entry(&desc, &entry)) {
		while ((index = sha1_array_lookup(found, entry.sha1)) >= 0)
			sha1_array_remove(found, index);

		if (S_ISDIR(entry.mode))
			remove_found_in_tree(lookup_tree(entry.sha1), found);
	}
}

static int commit_contains_any(struct commit *commit,
	struct sha1_array *to_find, struct strbuf *path)
{
	if (sha1_array_lookup(to_find, commit->tree->object.sha1) >= 0)
		return 1;
	return tree_contains_any(commit->tree, to_find, path);
}

static void find_all_contained(struct commit *commit,
	struct sha1_array *to_find, struct sha1_array *found)
{
	if (sha1_array_lookup(to_find, commit->tree->object.sha1) >= 0)
		sha1_array_append(found, commit->tree->object.sha1);
	find_all_contained_in_tree(commit->tree, to_find, found);
}

static void remove_found(struct commit* commit, struct sha1_array *found)
{
	int index;
	while ((index = sha1_array_lookup(found, commit->tree->object.sha1)) >= 0)
		sha1_array_remove(found, index);
	remove_found_in_tree(commit->tree, found);
}

static int commit_introduces_any(struct commit *commit,
	struct sha1_array *to_find)
{
	int original_count;
	struct sha1_array found = SHA1_ARRAY_INIT;

	find_all_contained(commit, to_find, &found);

	original_count = found.nr;

	struct commit_list *p;
	for (p = commit->parents; p; p = p->next) {
		if (!found.nr)
			break;
		remove_found(p->item, &found);
	}

	if (found.nr)
		fprintf(stderr, "%s contains %d interesting objects %d of which is new\n",
			sha1_to_hex(commit->object.sha1), original_count, found.nr);

	return found.nr;
}

static int opt_introduce;
static struct option builtin_find_objects_options[] = {
	OPT_BOOL('i', "introducing", &opt_introduce, "Only show commits that introduce the first of the objects to find"),
	OPT__VERBOSE(&verbose, "show object type and size"),
	OPT_END()
};

int cmd_find_blob(int argc, const char **argv, const char *prefix)
{
	struct rev_info rev = {};
	struct setup_revision_opt s_r_opt = {};
	struct commit *c;
	struct strbuf buf, path;
	unsigned char sha1[20];
	struct sha1_array to_find = SHA1_ARRAY_INIT;

	argc = parse_options(argc, argv, prefix, builtin_find_objects_options,
			     NULL, PARSE_OPT_KEEP_ARGV0);

	init_revisions(&rev, NULL);
	argc = setup_revisions(argc, argv, &rev, &s_r_opt);
	if (prepare_revision_walk(&rev)) {
		die(_("revision walk setup failed"));
	}

	strbuf_init(&buf, 0);
	while (strbuf_getline(&buf, stdin, '\n') != EOF) {
		if (!get_sha1_hex(buf.buf, sha1)) {
			sha1_array_append(&to_find, sha1);
		}
	}
	strbuf_release(&buf);

	strbuf_init(&path, PATH_MAX);
	while ((c = get_revision(&rev))) {
		if (opt_introduce) {
			if (!commit_introduces_any(c, &to_find))
				continue;
		}
		else {
			if (!commit_contains_any(c, &to_find, &path))
				continue;
		}

		printf("%s %s\n", sha1_to_hex(c->object.sha1), path.buf);
		strbuf_setlen(&path, 0);
	}
	strbuf_release(&path);
	return 0;
}
