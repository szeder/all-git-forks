#include "cache.h"
#include "refs.h"
#include "strbuf.h"
#include "worktree.h"

void free_worktrees(struct worktree **worktrees)
{
	int i;

	for (i = 0; worktrees[i]; i++) {
		free(worktrees[i]->path);
		free(worktrees[i]->git_dir);
		free(worktrees[i]->head_ref);
		free(worktrees[i]);
	}
	free (worktrees);
}

/*
 * read 'path_to_ref' into 'ref'.  Also if is_detached is not NULL,
 * set is_detached to 1 if the ref is detatched.
 *
 * $GIT_COMMON_DIR/$symref (e.g. HEAD) is practically outside $GIT_DIR so
 * for linked worktrees, `resolve_ref_unsafe()` won't work (it uses
 * git_path). Parse the ref ourselves.
 *
 * return -1 if the ref is not a proper ref, 0 otherwise (success)
 */
static int parse_ref(char *path_to_ref, struct strbuf *ref, int *is_detached)
{
	if (is_detached)
		*is_detached = 0;
	if (!strbuf_readlink(ref, path_to_ref, 0)) {
		if (!starts_with(ref->buf, "refs/")
				|| check_refname_format(ref->buf, 0))
			return -1;

	} else if (strbuf_read_file(ref, path_to_ref, 0) >= 0) {
		if (starts_with(ref->buf, "ref:")) {
			strbuf_remove(ref, 0, strlen("ref:"));
			strbuf_trim(ref);
			if (check_refname_format(ref->buf, 0))
				return -1;
		} else if (is_detached)
			*is_detached = 1;
	}
	return 0;
}

/**
 * Add the head_sha1 and head_ref (if not detached) to the given worktree
 */
static void add_head_info(struct strbuf *head_ref, struct worktree *worktree)
{
	if (head_ref->len) {
		if (worktree->is_detached) {
			get_sha1_hex(head_ref->buf, worktree->head_sha1);
		} else {
			resolve_ref_unsafe(head_ref->buf, 0, worktree->head_sha1, NULL);
			worktree->head_ref = strbuf_detach(head_ref, NULL);
		}
	}
}

/**
 * get the main worktree
 */
static struct worktree *get_main_worktree(void)
{
	struct worktree *worktree = NULL;
	struct strbuf path = STRBUF_INIT;
	struct strbuf worktree_path = STRBUF_INIT;
	struct strbuf git_dir = STRBUF_INIT;
	struct strbuf head_ref = STRBUF_INIT;
	int is_bare = 0;
	int is_detached = 0;

	strbuf_addf(&git_dir, "%s", absolute_path(get_git_common_dir()));
	strbuf_addf(&worktree_path, "%s", absolute_path(get_git_common_dir()));
	is_bare = !strbuf_strip_suffix(&worktree_path, "/.git");
	if (is_bare)
		strbuf_strip_suffix(&worktree_path, "/.");

	strbuf_addf(&path, "%s/HEAD", get_git_common_dir());

	if (!parse_ref(path.buf, &head_ref, &is_detached)) {
		worktree = xmalloc(sizeof(struct worktree));
		worktree->path = strbuf_detach(&worktree_path, NULL);
		worktree->git_dir = strbuf_detach(&git_dir, NULL);
		worktree->is_bare = is_bare;
		worktree->head_ref = NULL;
		worktree->is_detached = is_detached;
		add_head_info(&head_ref, worktree);
	}
	return worktree;
}

static struct worktree *get_linked_worktree(const char *id)
{
	struct worktree *worktree = NULL;
	struct strbuf path = STRBUF_INIT;
	struct strbuf worktree_path = STRBUF_INIT;
	struct strbuf git_dir = STRBUF_INIT;
	struct strbuf head_ref = STRBUF_INIT;
	int is_detached = 0;

	if (!id)
		goto done;

	strbuf_addf(&git_dir, "%s/worktrees/%s",
			absolute_path(get_git_common_dir()), id);
	strbuf_addf(&path, "%s/gitdir", git_dir.buf);
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
	strbuf_addf(&path, "%s/worktrees/%s/HEAD", get_git_common_dir(), id);

	if (parse_ref(path.buf, &head_ref, &is_detached))
		goto done;

	worktree = xmalloc(sizeof(struct worktree));
	worktree->path = strbuf_detach(&worktree_path, NULL);
	worktree->git_dir = strbuf_detach(&git_dir, NULL);
	worktree->is_bare = 0;
	worktree->head_ref = NULL;
	worktree->is_detached = is_detached;
	add_head_info(&head_ref, worktree);

done:
	strbuf_release(&path);
	strbuf_release(&git_dir);
	strbuf_release(&head_ref);
	strbuf_release(&worktree_path);
	return worktree;
}

/**
 * get the estimated worktree count for use in sizing the worktree array
 * Note that the minimum count should be 2 (main worktree + NULL).  Using the
 * file count in $GIT_COMMON_DIR/worktrees includes '.' and '..' so the
 * minimum is satisfied by counting those entries.
 */
static int get_estimated_worktree_count(void)
{
	struct strbuf path = STRBUF_INIT;
	DIR *dir;
	int count = 0;

	strbuf_addf(&path, "%s/worktrees", get_git_common_dir());
	dir = opendir(path.buf);
	strbuf_release(&path);
	if (dir) {
		while (readdir(dir))
			count++;
		closedir(dir);
	}

	if (!count)
		count = 2;

	return count;
}

struct worktree **get_worktrees(void)
{
	struct worktree **list = NULL;
	struct strbuf path = STRBUF_INIT;
	DIR *dir;
	struct dirent *d;
	int counter = 0;

	list = xmalloc(get_estimated_worktree_count() * sizeof(struct worktree *));

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
				list[counter++] = linked;
			}
		}
		closedir(dir);
	}
	list[counter] = NULL;
	return list;
}

char *find_shared_symref(const char *symref, const char *target)
{
	char *existing = NULL;
	struct strbuf path = STRBUF_INIT;
	struct strbuf sb = STRBUF_INIT;
	struct worktree **worktrees = get_worktrees();
	int symref_is_head = !strcmp("HEAD", symref);
	int i;

	for (i = 0; worktrees[i]; i++) {
		if (!symref_is_head) {
			strbuf_reset(&path);
			strbuf_reset(&sb);
			strbuf_addf(&path, "%s/%s", worktrees[i]->git_dir, symref);

			if (parse_ref(path.buf, &sb, NULL)) {
				continue;
			}

			if (!strcmp(sb.buf, target)) {
				existing = xstrdup(worktrees[i]->path);
				break;
			}
		} else {
			if (worktrees[i]->head_ref && !strcmp(worktrees[i]->head_ref, target)) {
				existing = xstrdup(worktrees[i]->path);
				break;
			}
		}
	}

	strbuf_release(&path);
	strbuf_release(&sb);
	free_worktrees(worktrees);

	return existing;
}
