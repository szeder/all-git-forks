#include "cache.h"
#include "refs.h"
#include "strbuf.h"
#include "worktree.h"
#include "dir.h"

void clear_worktree(struct worktree *wt)
{
	if (!wt)
		return;
	free(wt->path);
	free(wt->id);
	free(wt->head_ref);
	memset(wt, 0, sizeof(*wt));
}

void free_worktrees(struct worktree **worktrees)
{
	int i = 0;

	for (i = 0; worktrees[i]; i++) {
		clear_worktree(worktrees[i]);
		free(worktrees[i]);
	}
	free (worktrees);
}

/*
 * read 'path_to_ref' into 'ref'.  Also if is_detached is not NULL,
 * set is_detached to 1 (0) if the ref is detatched (is not detached).
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
		/* HEAD is symbolic link */
		if (!starts_with(ref->buf, "refs/") ||
				check_refname_format(ref->buf, 0))
			return -1;
	} else if (strbuf_read_file(ref, path_to_ref, 0) >= 0) {
		/* textual symref or detached */
		if (!starts_with(ref->buf, "ref:")) {
			if (is_detached)
				*is_detached = 1;
		} else {
			strbuf_remove(ref, 0, strlen("ref:"));
			strbuf_trim(ref);
			if (check_refname_format(ref->buf, 0))
				return -1;
		}
	} else
		return -1;
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
	struct strbuf head_ref = STRBUF_INIT;
	int is_bare = 0;
	int is_detached = 0;

	strbuf_addstr(&worktree_path, absolute_path(get_git_common_dir()));
	is_bare = !strbuf_strip_suffix(&worktree_path, "/.git");
	if (is_bare)
		strbuf_strip_suffix(&worktree_path, "/.");

	strbuf_addf(&path, "%s/HEAD", get_git_common_dir());

	if (parse_ref(path.buf, &head_ref, &is_detached) < 0)
		goto done;

	worktree = xmalloc(sizeof(struct worktree));
	worktree->path = strbuf_detach(&worktree_path, NULL);
	worktree->id = NULL;
	worktree->is_bare = is_bare;
	worktree->head_ref = NULL;
	worktree->is_detached = is_detached;
	add_head_info(&head_ref, worktree);

done:
	strbuf_release(&path);
	strbuf_release(&worktree_path);
	strbuf_release(&head_ref);
	return worktree;
}

static struct worktree *get_linked_worktree(const char *id)
{
	struct worktree *worktree = NULL;
	struct strbuf path = STRBUF_INIT;
	struct strbuf worktree_path = STRBUF_INIT;
	struct strbuf head_ref = STRBUF_INIT;
	int is_detached = 0;

	if (!id)
		die("Missing linked worktree name");

	strbuf_git_common_path(&path, "worktrees/%s/gitdir", id);
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

	if (parse_ref(path.buf, &head_ref, &is_detached) < 0)
		goto done;

	worktree = xmalloc(sizeof(struct worktree));
	worktree->path = strbuf_detach(&worktree_path, NULL);
	worktree->id = xstrdup(id);
	worktree->is_bare = 0;
	worktree->head_ref = NULL;
	worktree->is_detached = is_detached;
	add_head_info(&head_ref, worktree);

done:
	strbuf_release(&path);
	strbuf_release(&worktree_path);
	strbuf_release(&head_ref);
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
			if (is_dot_or_dotdot(d->d_name))
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

const char *get_worktree_git_dir(const struct worktree *wt)
{
	if (wt->id)
		return git_common_path("worktrees/%s", wt->id);
	else
		return get_git_common_dir();
}

struct worktree *find_worktree_by_path(struct worktree **list,
				       const char *path_)
{
	char *path = xstrdup(real_path(path_));
	struct worktree *wt = NULL;

	while (*list) {
		wt = *list++;
		if (!strcmp_icase(path, real_path(wt->path)))
			break;
		wt = NULL;
	}
	free(path);
	return wt;
}

int is_main_worktree(const struct worktree *wt)
{
	return wt && !wt->id;
}

static int report(int quiet, const char *fmt, ...)
{
	va_list params;

	if (quiet)
		return -1;

	va_start(params, fmt);
	vfprintf(stderr, fmt, params);
	fputc('\n', stderr);
	va_end(params);
	return -1;
}

int validate_worktree(const struct worktree *wt, int quiet)
{
	struct strbuf sb = STRBUF_INIT;
	const char *path;
	int err;

	if (is_main_worktree(wt)) {
		/*
		 * Main worktree using .git file to point to the
		 * repository would make it impossible to know where
		 * the actual worktree is if this function is executed
		 * from another worktree. No .git file support for now.
		 */
		strbuf_addf(&sb, "%s/.git", wt->path);
		if (!is_directory(sb.buf)) {
			strbuf_release(&sb);
			return report(quiet, _("'%s/.git' at main worktree is not the repository directory"),
				      wt->path);
		}
		return 0;
	}

	/*
	 * Make sure "gitdir" file points to a real .git file and that
	 * file points back here.
	 */
	if (!is_absolute_path(wt->path))
		return report(quiet, _("'%s' file does not contain absolute path to the worktree location"),
			      git_common_path("worktrees/%s/gitdir", wt->id));

	strbuf_addf(&sb, "%s/.git", wt->path);
	if (!file_exists(sb.buf)) {
		strbuf_release(&sb);
		return report(quiet, _("'%s/.git' does not exist"), wt->path);
	}

	path = read_gitfile_gently(sb.buf, &err);
	strbuf_release(&sb);
	if (!path)
		return report(quiet, _("'%s/.git' is not a .git file, error code %d"),
			      wt->path, err);

	if (strcmp_icase(path, real_path(git_common_path("worktrees/%s", wt->id))))
		return report(quiet, _("'%s' does not point back to"),
			      wt->path, git_common_path("worktrees/%s", wt->id));

	return 0;
}

int update_worktree_location(struct worktree *wt, const char *path_)
{
	struct strbuf path = STRBUF_INIT;
	int ret;

	if (is_main_worktree(wt))
		return 0;

	strbuf_add_absolute_path(&path, path_);
	if (strcmp_icase(wt->path, path.buf)) {
		if (!write_file_gently(git_common_path("worktrees/%s/gitdir",
						       wt->id),
				       "%s/.git", real_path(path.buf))) {
			free(wt->path);
			wt->path = strbuf_detach(&path, NULL);
			ret = 0;
		} else
			ret = sys_error(_("failed to update '%s'"),
					git_common_path("worktrees/%s/gitdir",
							wt->id));
	}
	strbuf_release(&path);
	return ret;
}

char *find_shared_symref(const char *symref, const char *target)
{
	char *existing = NULL;
	struct strbuf path = STRBUF_INIT;
	struct strbuf sb = STRBUF_INIT;
	struct worktree **worktrees = get_worktrees();
	int i = 0;

	for (i = 0; worktrees[i]; i++) {
		strbuf_reset(&path);
		strbuf_reset(&sb);
		strbuf_addf(&path, "%s/%s",
			    get_worktree_git_dir(worktrees[i]),
			    symref);

		if (parse_ref(path.buf, &sb, NULL)) {
			continue;
		}

		if (!strcmp(sb.buf, target)) {
			existing = xstrdup(worktrees[i]->path);
			break;
		}
	}

	strbuf_release(&path);
	strbuf_release(&sb);
	free_worktrees(worktrees);

	return existing;
}
