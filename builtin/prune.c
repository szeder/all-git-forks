#include "cache.h"
#include "commit.h"
#include "diff.h"
#include "revision.h"
#include "builtin.h"
#include "reachable.h"
#include "parse-options.h"
#include "progress.h"
#include "dir.h"

static const char * const prune_usage[] = {
	N_("git prune [-n] [-v] [--expire <time>] [--] [<head>...]"),
	NULL
};
static int show_only;
static int verbose;
static unsigned long expire;
static int show_progress = -1;

static int prune_tmp_file(const char *fullpath)
{
	struct stat st;
	if (lstat(fullpath, &st))
		return error("Could not stat '%s'", fullpath);
	if (st.st_mtime > expire)
		return 0;
	if (show_only || verbose)
		printf("Removing stale temporary file %s\n", fullpath);
	if (!show_only)
		unlink_or_warn(fullpath);
	return 0;
}

static int prune_object(const char *fullpath, const unsigned char *sha1)
{
	struct stat st;
	if (lstat(fullpath, &st))
		return error("Could not stat '%s'", fullpath);
	if (st.st_mtime > expire)
		return 0;
	if (show_only || verbose) {
		enum object_type type = sha1_object_info(sha1, NULL);
		printf("%s %s\n", sha1_to_hex(sha1),
		       (type > 0) ? typename(type) : "unknown");
	}
	if (!show_only)
		unlink_or_warn(fullpath);
	return 0;
}

static int prune_dir(int i, struct strbuf *path)
{
	size_t baselen = path->len;
	DIR *dir = opendir(path->buf);
	struct dirent *de;

	if (!dir)
		return 0;

	while ((de = readdir(dir)) != NULL) {
		char name[100];
		unsigned char sha1[20];

		if (is_dot_or_dotdot(de->d_name))
			continue;
		if (strlen(de->d_name) == 38) {
			sprintf(name, "%02x", i);
			memcpy(name+2, de->d_name, 39);
			if (get_sha1_hex(name, sha1) < 0)
				break;

			/*
			 * Do we know about this object?
			 * It must have been reachable
			 */
			if (lookup_object(sha1))
				continue;

			strbuf_addf(path, "/%s", de->d_name);
			prune_object(path->buf, sha1);
			strbuf_setlen(path, baselen);
			continue;
		}
		if (starts_with(de->d_name, "tmp_obj_")) {
			strbuf_addf(path, "/%s", de->d_name);
			prune_tmp_file(path->buf);
			strbuf_setlen(path, baselen);
			continue;
		}
		fprintf(stderr, "bad sha1 file: %s/%s\n", path->buf, de->d_name);
	}
	closedir(dir);
	if (!show_only)
		rmdir(path->buf);
	return 0;
}

static void prune_object_dir(const char *path)
{
	struct strbuf buf = STRBUF_INIT;
	size_t baselen;
	int i;

	strbuf_addstr(&buf, path);
	strbuf_addch(&buf, '/');
	baselen = buf.len;

	for (i = 0; i < 256; i++) {
		strbuf_addf(&buf, "%02x", i);
		prune_dir(i, &buf);
		strbuf_setlen(&buf, baselen);
	}
}

static int prune_repo_dir(const char *id, struct stat *st, struct strbuf *reason)
{
	char *path;
	int fd, len;

	if (!is_directory(git_path("repos/%s", id))) {
		strbuf_addf(reason, _("Removing repos/%s: not a valid directory"), id);
		return 1;
	}
	if (file_exists(git_path("repos/%s/locked", id)))
		return 0;
	if (stat(git_path("repos/%s/gitdir", id), st)) {
		st->st_mtime = expire;
		strbuf_addf(reason, _("Removing repos/%s: gitdir file does not exist"), id);
		return 1;
	}
	fd = open(git_path("repos/%s/gitdir", id), O_RDONLY);
	if (fd < 0) {
		st->st_mtime = expire;
		strbuf_addf(reason, _("Removing repos/%s: unable to read gitdir file (%s)"),
			    id, strerror(errno));
		return 1;
	}
	len = st->st_size;
	path = xmalloc(len + 1);
	read_in_full(fd, path, len);
	close(fd);
	while (len && (path[len - 1] == '\n' || path[len - 1] == '\r'))
		len--;
	if (!len) {
		st->st_mtime = expire;
		strbuf_addf(reason, _("Removing repos/%s: invalid gitdir file"), id);
		free(path);
		return 1;
	}
	path[len] = '\0';
	if (!file_exists(path)) {
		struct stat st_link;
		free(path);
		/*
		 * the repo is moved manually and has not been
		 * accessed since?
		 */
		if (!stat(git_path("repos/%s/link", id), &st_link) &&
		    st_link.st_nlink > 1)
			return 0;
		strbuf_addf(reason, _("Removing repos/%s: gitdir file points to non-existent location"), id);
		return 1;
	}
	free(path);
	return 0;
}

static void prune_repos_dir(void)
{
	struct strbuf reason = STRBUF_INIT;
	struct strbuf path = STRBUF_INIT;
	DIR *dir = opendir(git_path("repos"));
	struct dirent *d;
	int ret;
	struct stat st;
	if (!dir)
		return;
	while ((d = readdir(dir)) != NULL) {
		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;
		strbuf_reset(&reason);
		if (!prune_repo_dir(d->d_name, &st, &reason) ||
		    st.st_mtime > expire)
			continue;
		if (show_only || verbose)
			printf("%s\n", reason.buf);
		if (show_only)
			continue;
		strbuf_reset(&path);
		strbuf_addstr(&path, git_path("repos/%s", d->d_name));
		ret = remove_dir_recursively(&path, 0);
		if (ret < 0 && errno == ENOTDIR)
			ret = unlink(path.buf);
		if (ret)
			error(_("failed to remove: %s"), strerror(errno));
	}
	closedir(dir);
	if (!show_only)
		rmdir(git_path("repos"));
	strbuf_release(&reason);
	strbuf_release(&path);
}

/*
 * Write errors (particularly out of space) can result in
 * failed temporary packs (and more rarely indexes and other
 * files beginning with "tmp_") accumulating in the object
 * and the pack directories.
 */
static void remove_temporary_files(const char *path)
{
	DIR *dir;
	struct dirent *de;

	dir = opendir(path);
	if (!dir) {
		fprintf(stderr, "Unable to open directory %s\n", path);
		return;
	}
	while ((de = readdir(dir)) != NULL)
		if (starts_with(de->d_name, "tmp_"))
			prune_tmp_file(mkpath("%s/%s", path, de->d_name));
	closedir(dir);
}

int cmd_prune(int argc, const char **argv, const char *prefix)
{
	struct rev_info revs;
	struct progress *progress = NULL;
	int prune_repos = 0;
	const struct option options[] = {
		OPT__DRY_RUN(&show_only, N_("do not remove, show only")),
		OPT__VERBOSE(&verbose, N_("report pruned objects")),
		OPT_BOOL(0, "progress", &show_progress, N_("show progress")),
		OPT_BOOL(0, "repos", &prune_repos, N_("prune .git/repos/")),
		OPT_EXPIRY_DATE(0, "expire", &expire,
				N_("expire objects older than <time>")),
		OPT_END()
	};
	char *s;

	expire = ULONG_MAX;
	save_commit_buffer = 0;
	check_replace_refs = 0;
	init_revisions(&revs, prefix);

	argc = parse_options(argc, argv, prefix, options, prune_usage, 0);

	if (prune_repos) {
		if (argc)
			die(_("--repos does not take extra arguments"));
		prune_repos_dir();
		return 0;
	}

	while (argc--) {
		unsigned char sha1[20];
		const char *name = *argv++;

		if (!get_sha1(name, sha1)) {
			struct object *object = parse_object_or_die(sha1, name);
			add_pending_object(&revs, object, "");
		}
		else
			die("unrecognized argument: %s", name);
	}

	if (show_progress == -1)
		show_progress = isatty(2);
	if (show_progress)
		progress = start_progress_delay(_("Checking connectivity"), 0, 0, 2);

	mark_reachable_objects(&revs, 1, progress);
	stop_progress(&progress);
	prune_object_dir(get_object_directory());

	prune_packed_objects(show_only ? PRUNE_PACKED_DRY_RUN : 0);
	remove_temporary_files(get_object_directory());
	s = mkpathdup("%s/pack", get_object_directory());
	remove_temporary_files(s);
	free(s);

	if (is_repository_shallow())
		prune_shallow(show_only);

	return 0;
}
