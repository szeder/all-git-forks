#include "cache.h"
#include "refs.h"
#include "strbuf.h"
#include "worktree.h"

void free_worktrees(struct worktree **worktrees)
{
	int i = 0;

	for (i = 0; worktrees[i]; i++) {
		free(worktrees[i]->path);
		free(worktrees[i]->git_dir);
		free(worktrees[i]->head_ref);
		free(worktrees[i]);
	}
	free (worktrees);
}

/**
 * Add the is_detached, head_sha1 and head_ref (if not detached) to the given worktree
 */
static void add_head_info(const char *head_ref, const char *sha1, struct worktree *worktree)
{
	worktree->is_detached = !is_null_sha1(sha1);
	if (worktree->is_detached) {
		hashcpy(worktree->head_sha1, sha1);
		worktree->head_ref = NULL;
	} else {
		resolve_ref_unsafe(head_ref, 0, worktree->head_sha1, NULL);
		worktree->head_ref = xstrdup(head_ref);
	}
}

/**
 * get the main worktree
 */
static struct worktree *get_main_worktree(void)
{
	struct worktree *worktree = NULL;
	struct strbuf worktree_path = STRBUF_INIT;
	struct strbuf gitdir = STRBUF_INIT;
	const char *head_ref;
	char sha1[GIT_SHA1_RAWSZ];
	int is_bare = 0;

	strbuf_addf(&gitdir, "%s", absolute_path(get_git_common_dir()));
	strbuf_addbuf(&worktree_path, &gitdir);
	is_bare = !strbuf_strip_suffix(&worktree_path, "/.git");
	if (is_bare)
		strbuf_strip_suffix(&worktree_path, "/.");

	head_ref = resolve_ref_unsafe("HEAD",
			RESOLVE_REF_NO_RECURSE | RESOLVE_REF_COMMON_DIR,
			sha1, NULL);
	if (!head_ref)
		goto done;

	worktree = xmalloc(sizeof(struct worktree));
	worktree->path = strbuf_detach(&worktree_path, NULL);
	worktree->git_dir = strbuf_detach(&gitdir, NULL);
	worktree->is_bare = is_bare;
	add_head_info(head_ref, sha1, worktree);

done:
	strbuf_release(&gitdir);
	strbuf_release(&worktree_path);
	return worktree;
}

static struct worktree *get_linked_worktree(const char *id)
{
	struct worktree *worktree = NULL;
	struct strbuf path = STRBUF_INIT;
	struct strbuf worktree_path = STRBUF_INIT;
	struct strbuf gitdir = STRBUF_INIT;
	const char *head_ref;
	char sha1[GIT_SHA1_RAWSZ];

	if (!id)
		die("Missing linked worktree name");

	strbuf_addf(&gitdir, "%s/worktrees/%s",
			absolute_path(get_git_common_dir()), id);
	strbuf_addf(&path, "%s/gitdir", gitdir.buf);
	if (strbuf_read_file(&worktree_path, path.buf, 0) <= 0)
		/* invalid gitdir file */
		goto done;

	strbuf_rtrim(&worktree_path);
	if (!strbuf_strip_suffix(&worktree_path, "/.git")) {
		strbuf_reset(&worktree_path);
		strbuf_addstr(&worktree_path, absolute_path("."));
		strbuf_strip_suffix(&worktree_path, "/.");
	}

	strbuf_reset(&path);
	strbuf_addf(&path, "worktrees/%s/HEAD", id);
	head_ref = resolve_ref_unsafe(path.buf,
			RESOLVE_REF_NO_RECURSE | RESOLVE_REF_COMMON_DIR,
			sha1, NULL);
	if (!head_ref)
		goto done;

	worktree = xmalloc(sizeof(struct worktree));
	worktree->path = strbuf_detach(&worktree_path, NULL);
	worktree->git_dir = strbuf_detach(&gitdir, NULL);
	worktree->is_bare = 0;
	add_head_info(head_ref, sha1, worktree);

done:
	strbuf_release(&path);
	strbuf_release(&gitdir);
	strbuf_release(&worktree_path);
	return worktree;
}

struct worktree **get_worktrees(void)
{
	struct worktree **list = NULL;
	struct strbuf path = STRBUF_INIT;
	DIR *dir;
	struct dirent *d;
	int counter = 0, alloc = 2;

	list = xmalloc(alloc * sizeof(struct worktree *));

	if ((list[counter] = get_main_worktree()))
		counter++;

	strbuf_addf(&path, "%s/worktrees", get_git_common_dir());
	dir = opendir(path.buf);
	strbuf_release(&path);
	if (dir) {
		while ((d = readdir(dir)) != NULL) {
			struct worktree *linked = NULL;
			if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
				continue;

			if ((linked = get_linked_worktree(d->d_name))) {
				ALLOC_GROW(list, counter + 1, alloc);
				list[counter++] = linked;
			}
		}
		closedir(dir);
	}
	ALLOC_GROW(list, counter + 1, alloc);
	list[counter] = NULL;
	return list;
}

char *find_shared_symref(const char *symref, const char *target)
{
	char *existing = NULL;
	struct strbuf path = STRBUF_INIT;
	struct worktree **worktrees = get_worktrees();
	const char *resolved;
	int i = 0;
	char sha1[GIT_SHA1_RAWSZ];
	int common_prefix_len = strlen(absolute_path(get_git_common_dir())) + 1;

	for (i = 0; worktrees[i]; i++) {
		strbuf_reset(&path);
		strbuf_addf(&path, "%s/%s", worktrees[i]->git_dir, symref);
		strbuf_remove(&path, 0, common_prefix_len);

		resolved = resolve_ref_unsafe(path.buf,
				RESOLVE_REF_NO_RECURSE | RESOLVE_REF_COMMON_DIR,
				sha1, NULL);
		if (resolved && !strcmp(resolved, target)) {
			existing = xstrdup(worktrees[i]->path);
			break;
		}
	}

	strbuf_release(&path);
	free_worktrees(worktrees);

	return existing;
}
