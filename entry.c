#include "cache.h"
#include "blob.h"
#include "dir.h"
#include "streaming.h"
#include <string.h>
#include <stdio.h>

int nnn;
int xlink(const char *from, const char *to)
{
	int ret = link(from,to);
	printf("#%d,link %s,%s, ret=%d, errno=%d\n", nnn++, from, to, ret, errno);
	return ret;
}
#define link(s,e) xlink(s,e)

static void create_directories(const char *path, int path_len,
			       const struct checkout *state)
{
	char *buf = xmalloc(path_len + 1);
	int len = 0;

	while (len < path_len) {
		do {
			buf[len] = path[len];
			len++;
		} while (len < path_len && path[len] != '/');
		if (len >= path_len)
			break;
		buf[len] = 0;

		/*
		 * For 'checkout-index --prefix=<dir>', <dir> is
		 * allowed to be a symlink to an existing directory,
		 * and we set 'state->base_dir_len' below, such that
		 * we test the path components of the prefix with the
		 * stat() function instead of the lstat() function.
		 */
		if (has_dirs_only_path(buf, len, state->base_dir_len))
			continue; /* ok, it is already a directory. */

		/*
		 * If this mkdir() would fail, it could be that there
		 * is already a symlink or something else exists
		 * there, therefore we then try to unlink it and try
		 * one more time to create the directory.
		 */
		if (mkdir(buf, 0777)) {
			if (errno == EEXIST && state->force &&
			    !unlink_or_warn(buf) && !mkdir(buf, 0777))
				continue;
			die_errno("cannot create directory at '%s'", buf);
		}
	}
	free(buf);
}

static void remove_subtree(const char *path)
{
	DIR *dir = opendir(path);
	struct dirent *de;
	char pathbuf[PATH_MAX];
	char *name;

	if (!dir)
		die_errno("cannot opendir '%s'", path);
	strcpy(pathbuf, path);
	name = pathbuf + strlen(path);
	*name++ = '/';
	while ((de = readdir(dir)) != NULL) {
		struct stat st;
		if (is_dot_or_dotdot(de->d_name))
			continue;
		strcpy(name, de->d_name);
		if (lstat(pathbuf, &st))
			die_errno("cannot lstat '%s'", pathbuf);
		if (S_ISDIR(st.st_mode))
			remove_subtree(pathbuf);
		else if (unlink(pathbuf))
			die_errno("cannot unlink '%s'", pathbuf);
	}
	closedir(dir);
	if (rmdir(path))
		die_errno("cannot rmdir '%s'", path);
}

static int create_file(const char *path, unsigned int mode)
{
	mode = (mode & 0100) ? 0777 : 0666;
	return open(path, O_WRONLY | O_CREAT | O_EXCL, mode);
}

static void *read_blob_entry(struct cache_entry *ce, unsigned long *size)
{
	enum object_type type;
	void *new = read_sha1_file(ce->sha1, &type, size);

	if (new) {
		if (type == OBJ_BLOB)
			return new;
		free(new);
	}
	return NULL;
}

static int open_output_fd(char *path, struct cache_entry *ce, int to_tempfile)
{
	int symlink = (ce->ce_mode & S_IFMT) != S_IFREG;
	if (to_tempfile) {
		strcpy(path, symlink
		       ? ".merge_link_XXXXXX" : ".merge_file_XXXXXX");
		return mkstemp(path);
	} else {
		return create_file(path, !symlink ? ce->ce_mode : 0666);
	}
}

static int fstat_output(int fd, const struct checkout *state, struct stat *st)
{
	/* use fstat() only when path == ce->name */
	if (fstat_is_reliable() &&
	    state->refresh_cache && !state->base_dir_len) {
		fstat(fd, st);
		return 1;
	}
	return 0;
}

static int streaming_write_entry(struct cache_entry *ce, char *path,
				 struct stream_filter *filter,
				 const struct checkout *state, int to_tempfile,
				 int *fstat_done, struct stat *statbuf)
{
	int result = -1;
	int fd;

	fd = open_output_fd(path, ce, to_tempfile);
	if (0 <= fd) {
		result = stream_blob_to_fd(fd, ce->sha1, filter, 1);
		*fstat_done = fstat_output(fd, state, statbuf);
		result = close(fd);
	}
	if (result && 0 <= fd)
		unlink(path);
	return result;
}

struct postlink {
	struct cache_entry *ce;
	char *from,*to;
};
static struct postlink *postlinks = NULL;
static int npostlinks = 0;
static int maxpostlinks = 0;
static int resolved = 0;
int lasttry = 0;

void add_postlink(struct cache_entry *ce, char *from, char *to)
{
	if (npostlinks >= maxpostlinks) {
		if (maxpostlinks == 0)
			maxpostlinks = 64;
		else
			maxpostlinks *= 2;
		postlinks = realloc(postlinks, maxpostlinks*sizeof(struct postlink));
	}
	postlinks[npostlinks].ce = ce;
	postlinks[npostlinks].to = xstrdup(to);
	postlinks[npostlinks].from = xstrdup(from);
	npostlinks++;
}

void release_postlink(void)
{
	int i;
	for (i = 0; i < npostlinks; ++i) {
		free(postlinks[i].from);
		free(postlinks[i].to);
	}
	free(postlinks);
	postlinks = NULL;
	npostlinks = 0;
	maxpostlinks = 0;
}

static int recursive_link(const char *src, const char *dst) {
	// printf("recursive_link %s %s\n", src, dst);
	struct stat buf;
	struct dirent *dp;
	struct strbuf dstb = STRBUF_INIT;
	struct strbuf srcb = STRBUF_INIT;
	int err = stat(dst, &buf);
	DIR *d = opendir(src);
	if (!d)
		return ENOENT;
	if (err && errno == ENOENT) {
		err = mkdir(dst, 0777);
		if (err)
			return err;
		else
			resolved++;
	} else if (!(buf.st_mode & S_IFDIR))
		return EEXIST;
	err = 0;
	while ((dp = readdir(d)) != NULL) {
		if (is_dot_or_dotdot(dp->d_name))
			continue;
		strbuf_add(&dstb, dst, strlen(dst));
		strbuf_add(&dstb, "/", 1);
		strbuf_add(&dstb, dp->d_name, strlen(dp->d_name));
		strbuf_add(&srcb, src, strlen(src));
		strbuf_add(&srcb, "/", 1);
		strbuf_add(&srcb, dp->d_name, strlen(dp->d_name));
		if (!stat(srcb.buf, &buf)) {
			if (buf.st_mode & S_IFDIR) {
				if (recursive_link(srcb.buf, dstb.buf)) {
					err = -1;
				}
			} else {
				int r1,r2;
				r1 = unlink(dstb.buf);
				if (r1)
					error("unlink %s = %d, errno=%d\n", dstb.buf, r1, errno);
				r2 = link(srcb.buf, dstb.buf);
				if (r2) {
					if (maxpostlinks >= 0) {
						error("cannot link %s to %s", srcb.buf, dstb.buf);
					}
					err = -1;
				}
				if (r1 && !r2)
					resolved++;
			}
		} else {
			if (maxpostlinks >= 0) {
				error("cannot link %s to %s", srcb.buf, dstb.buf);
			}
			err = -1;
		}
		strbuf_reset(&srcb);
		strbuf_reset(&dstb);
	}
	strbuf_release(&dstb);
	strbuf_release(&srcb);
	closedir(d);
	return err;
}

static void resolvelink(struct strbuf *src, struct strbuf *dst)
{
	if (0 == strncmp(src->buf, "../", 3)) {
		char *p = dst->buf + dst->len - 1;
		while (p > dst->buf && *p != '/')
			--p;
		if (*p != '/')
			return;
		--p;
		while (p > dst->buf && *p != '/')
			--p;
		strbuf_remove(src, 0, 3);
		if (*p == '/')
			strbuf_insert(src, 0, dst->buf, p - dst->buf + 1);
		resolvelink(src, dst);
	} else if (0 == strncmp(src->buf, "./", 2)) {
		strbuf_remove(src, 0, 2);
		resolvelink(src, dst);
	}
}

static int fakesymlink(const char *old, const char *new)
{
	struct stat buf;
	int st;
	int err;
	char dir[MAXPATHLEN];
	char cdir[MAXPATHLEN];
	char *dend;
	struct strbuf src = STRBUF_INIT;
	struct strbuf dst = STRBUF_INIT;
	strcpy(dir, new);
	dend = strrchr(dir, '/');
	if (dend) {
		*dend = 0;
		if (!getcwd(cdir, MAXPATHLEN))
			return -1;
	}
	strbuf_add(&src, old, strlen(old));
	strbuf_add(&dst, new, strlen(new));
	resolvelink(&src, &dst);
	if (0 == (st = stat(src.buf, &buf))) {
		if (buf.st_mode & (S_IFREG | S_IFLNK)) {
			int r = unlink(dst.buf);
			err = link(src.buf, dst.buf);
			if (!err && !r)
				resolved++;
			if (err)
				error("Failed to link %s %s\n", src.buf, dst.buf);
		} else if (buf.st_mode & S_IFDIR) {
			err = recursive_link(src.buf, dst.buf);
			if (maxpostlinks < 0)
				err = 1;
//			printf("%d <- recursive_link\n", err);
		} else {
			error("mode:Failed to link %s %s\n", src.buf, dst.buf);
			err = -1;
		}
	} else {
		error("stat:Failed to link %s %s\n", src.buf, dst.buf);
		err = -1;
	}
	strbuf_release(&src);
	strbuf_release(&dst);
	return err;
}

int checkout_remaining_link_copies(void)
{
	int j;
	int err = 0;
	maxpostlinks = -1;
	resolved = 1;

	/* stupid, each iteration should resolve at least one entry */
	do {
		if (resolved == 0)
			lasttry = 1;
		resolved = 0;
		for (j = 0; j < npostlinks; ++j) {
			struct postlink *e = &postlinks[j];
			if (e->from) {
				if (0 == fakesymlink(e->from, e->to)) {
					if (e->ce) {
						struct stat st;
						if (0 == stat(e->to, &st))
							fill_stat_cache_info(e->ce, &st);
					}
					free(e->from);
					e->from = NULL;
				} else {
					if (lasttry) {
						err = 1;
					}
				}
			}
		}
	} while (resolved > 0 && !lasttry);
	release_postlink();
	return err;
}

static int write_entry(struct cache_entry *ce, char *path, const struct checkout *state, int to_tempfile)
{
	unsigned int ce_mode_s_ifmt = ce->ce_mode & S_IFMT;
	int fd, ret, fstat_done = 0;
	char *new;
	struct strbuf buf = STRBUF_INIT;
	unsigned long size;
	size_t wrote, newsize = 0;
	struct stat st;

	if (ce_mode_s_ifmt == S_IFREG) {
		struct stream_filter *filter = get_stream_filter(path, ce->sha1);
		if (filter &&
		    !streaming_write_entry(ce, path, filter,
					   state, to_tempfile,
					   &fstat_done, &st))
			goto finish;
	}

	switch (ce_mode_s_ifmt) {
	case S_IFREG:
	case S_IFLNK:
		new = read_blob_entry(ce, &size);
		if (!new)
			return error("unable to read sha1 file of %s (%s)",
				path, sha1_to_hex(ce->sha1));

		if (ce_mode_s_ifmt == S_IFLNK && (has_symlinks || copy_symlinks) && !to_tempfile) {
			if (copy_symlinks) {
				ret = fakesymlink(new, path);
				if (ret < 0) {
					// Could not copy now, assume we can do it later
					add_postlink(ce, new, path);
					ret = 0;
				}
			} else
				ret = symlink(new, path);
			free(new);
			if (ret)
				return error("unable to create symlink %s (%s)",
					     path, strerror(errno));
			break;
		}

		/*
		 * Convert from git internal format to working tree format
		 */
		if (ce_mode_s_ifmt == S_IFREG &&
		    convert_to_working_tree(ce->name, new, size, &buf)) {
			free(new);
			new = strbuf_detach(&buf, &newsize);
			size = newsize;
		}

		fd = open_output_fd(path, ce, to_tempfile);
		if (fd < 0) {
			free(new);
			return error("unable to create file %s (%s)",
				path, strerror(errno));
		}

		wrote = write_in_full(fd, new, size);
		if (!to_tempfile)
			fstat_done = fstat_output(fd, state, &st);
		close(fd);
		free(new);
		if (wrote != size)
			return error("unable to write file %s", path);
		break;
	case S_IFGITLINK:
		if (to_tempfile)
			return error("cannot create temporary subproject %s", path);
		if (mkdir(path, 0777) < 0)
			return error("cannot create subproject directory %s", path);
		break;
	default:
		return error("unknown file mode for %s in index", path);
	}

finish:
	if (state->refresh_cache) {
		if (!fstat_done)
			lstat(ce->name, &st);
		fill_stat_cache_info(ce, &st);
	}
	return 0;
}

/*
 * This is like 'lstat()', except it refuses to follow symlinks
 * in the path, after skipping "skiplen".
 */
static int check_path(const char *path, int len, struct stat *st, int skiplen)
{
	const char *slash = path + len;

	while (path < slash && *slash != '/')
		slash--;
	if (!has_dirs_only_path(path, slash - path, skiplen)) {
		errno = ENOENT;
		return -1;
	}
	return lstat(path, st);
}

int checkout_entry(struct cache_entry *ce, const struct checkout *state, char *topath)
{
	static char path[PATH_MAX + 1];
	struct stat st;
	int len = state->base_dir_len;

	if (topath)
		return write_entry(ce, topath, state, 1);

	memcpy(path, state->base_dir, len);
	strcpy(path + len, ce->name);
	len += ce_namelen(ce);

	if (!check_path(path, len, &st, state->base_dir_len)) {
		unsigned changed = ce_match_stat(ce, &st, CE_MATCH_IGNORE_VALID|CE_MATCH_IGNORE_SKIP_WORKTREE);
		if (!changed)
			return 0;
		if (!state->force) {
			if (!state->quiet)
				fprintf(stderr, "%s already exists, no checkout\n", path);
			return -1;
		}

		/*
		 * We unlink the old file, to get the new one with the
		 * right permissions (including umask, which is nasty
		 * to emulate by hand - much easier to let the system
		 * just do the right thing)
		 */
		if (S_ISDIR(st.st_mode)) {
			/* If it is a gitlink, leave it alone! */
			if (S_ISGITLINK(ce->ce_mode))
				return 0;
			if (!state->force)
				return error("%s is a directory", path);
			remove_subtree(path);
		} else {
			if (unlink(path)) {
//				printf("has_symlinks=%d, copy_symlinks=%d\n", has_symlinks, copy_symlinks);
				if (copy_symlinks) {
					remove_subtree(path);
				} else {
					return error("unable to unlink old '%s' (%s)", path, strerror(errno));
				}
			}
		}
	} else if (state->not_new)
		return 0;
	create_directories(path, len, state);
	return write_entry(ce, path, state, 0);
}
