#include "cache.h"
#include "dir.h"
#include "hashmap.h"

int copy_fd(int ifd, int ofd)
{
	while (1) {
		char buffer[8192];
		ssize_t len = xread(ifd, buffer, sizeof(buffer));
		if (!len)
			break;
		if (len < 0)
			return COPY_READ_ERROR;
		if (write_in_full(ofd, buffer, len) < 0)
			return COPY_WRITE_ERROR;
	}
	return 0;
}

static int copy_times(const char *dst, const char *src)
{
	struct stat st;
	struct utimbuf times;
	if (stat(src, &st) < 0)
		return -1;
	times.actime = st.st_atime;
	times.modtime = st.st_mtime;
	if (utime(dst, &times) < 0)
		return -1;
	return 0;
}

int copy_file(const char *dst, const char *src, int mode)
{
	int fdi, fdo, status;

	mode = (mode & 0111) ? 0777 : 0666;
	if ((fdi = open(src, O_RDONLY)) < 0)
		return fdi;
	if ((fdo = open(dst, O_WRONLY | O_CREAT | O_EXCL, mode)) < 0) {
		close(fdi);
		return fdo;
	}
	status = copy_fd(fdi, fdo);
	switch (status) {
	case COPY_READ_ERROR:
		error_errno("copy-fd: read returned");
		break;
	case COPY_WRITE_ERROR:
		error_errno("copy-fd: write returned");
		break;
	}
	close(fdi);
	if (close(fdo) != 0)
		return error_errno("%s: close error", dst);

	if (!status && adjust_shared_perm(dst))
		return -1;

	return status;
}

int copy_file_with_time(const char *dst, const char *src, int mode)
{
	int status = copy_file(dst, src, mode);
	if (!status)
		return copy_times(dst, src);
	return status;
}

struct inode_key {
	struct hashmap_entry entry;
	ino_t ino;
	dev_t dev;
	/*
	 * Reportedly, on cramfs a file and a dir can have same ino.
	 * Need to also remember "file/dir" bit:
	 */
	char isdir; /* bool */
};

struct inode_value {
	struct inode_key key;
	char name[FLEX_ARRAY];
};

#define HASH_SIZE      311u   /* Should be prime */
static inline unsigned hash_inode(ino_t i)
{
	return i % HASH_SIZE;
}

static int inode_cmp(const void *entry, const void *entry_or_key,
		     const void *keydata)
{
	const struct inode_value *inode = entry;
	const struct inode_key   *key   = entry_or_key;

	return !(inode->key.ino   == key->ino &&
		 inode->key.dev   == key->dev &&
		 inode->key.isdir == key->isdir);
}

static const char *is_in_ino_dev_hashtable(const struct hashmap *map,
					   const struct stat *st)
{
	struct inode_key key;
	struct inode_value *value;

	key.entry.hash = hash_inode(st->st_ino);
	key.ino	       = st->st_ino;
	key.dev	       = st->st_dev;
	key.isdir      = !!S_ISDIR(st->st_mode);
	value	       = hashmap_get(map, &key, NULL);
	return value ? value->name : NULL;
}

static void add_to_ino_dev_hashtable(struct hashmap *map,
				     const struct stat *st,
				     const char *path)
{
	struct inode_value *v;
	int len = strlen(path);

	v = xmalloc(offsetof(struct inode_value, name) + len + 1);
	v->key.entry.hash = hash_inode(st->st_ino);
	v->key.ino	  = st->st_ino;
	v->key.dev	  = st->st_dev;
	v->key.isdir      = !!S_ISDIR(st->st_mode);
	memcpy(v->name, path, len + 1);
	hashmap_add(map, v);
}

/*
 * Find out if the last character of a string matches the one given.
 * Don't underrun the buffer if the string length is 0.
 */
static inline char *last_char_is(const char *s, int c)
{
	if (s && *s) {
		size_t sz = strlen(s) - 1;
		s += sz;
		if ( (unsigned char)*s == c)
			return (char*)s;
	}
	return NULL;
}

static inline char *concat_path_file(const char *path, const char *filename)
{
	struct strbuf sb = STRBUF_INIT;
	char *lc;

	if (!path)
		path = "";
	lc = last_char_is(path, '/');
	while (*filename == '/')
		filename++;
	strbuf_addf(&sb, "%s%s%s", path, (lc==NULL ? "/" : ""), filename);
	return strbuf_detach(&sb, NULL);
}

static char *concat_subpath_file(const char *path, const char *f)
{
	if (f && is_dot_or_dotdot(f))
		return NULL;
	return concat_path_file(path, f);
}

static int do_unlink(const char *dest)
{
	int e = errno;

	if (unlink(dest) < 0) {
		errno = e; /* do not use errno from unlink */
		return error_errno(_("can't create '%s'"), dest);
	}
	return 0;
}

static int copy_dir_1(struct hashmap *inode_map,
		      const char *source,
		      const char *dest)
{
	/* This is a recursive function, try to minimize stack usage */
	struct stat source_stat;
	struct stat dest_stat;
	int retval = 0;
	int dest_exists = 0;
	int ovr;

	if (lstat(source, &source_stat) < 0)
		return error_errno(_("can't stat '%s'"), source);

	if (lstat(dest, &dest_stat) < 0) {
		if (errno != ENOENT)
			return error_errno(_("can't stat '%s'"), dest);
	} else {
		if (source_stat.st_dev == dest_stat.st_dev &&
		    source_stat.st_ino == dest_stat.st_ino)
			return error(_("'%s' and '%s' are the same file"), source, dest);
		dest_exists = 1;
	}

	if (S_ISDIR(source_stat.st_mode)) {
		DIR *dp;
		const char *tp;
		struct dirent *d;
		mode_t saved_umask = 0;

		/* Did we ever create source ourself before? */
		tp = is_in_ino_dev_hashtable(inode_map, &source_stat);
		if (tp)
			/* We did! it's a recursion! man the lifeboats... */
			return error(_("recursion detected, omitting directory '%s'"),
				     source);

		if (dest_exists) {
			if (!S_ISDIR(dest_stat.st_mode))
				return error(_("target '%s' is not a directory"), dest);
			/*
			 * race here: user can substitute a symlink between
			 * this check and actual creation of files inside dest
			 */
		} else {
			/* Create DEST */
			mode_t mode;
			saved_umask = umask(0);

			mode = source_stat.st_mode;
			/* Allow owner to access new dir (at least for now) */
			mode |= S_IRWXU;
			if (mkdir(dest, mode) < 0) {
				umask(saved_umask);
				return error_errno(_("can't create directory '%s'"), dest);
			}
			umask(saved_umask);
			/* need stat info for add_to_ino_dev_hashtable */
			if (lstat(dest, &dest_stat) < 0)
				return error_errno(_("can't stat '%s'"), dest);
		}

		/*
		 * remember (dev,inode) of each created dir. name is
		 * not remembered
		 */
		add_to_ino_dev_hashtable(inode_map, &dest_stat, "");

		/* Recursively copy files in SOURCE */
		dp = opendir(source);
		if (!dp) {
			retval = -1;
			goto preserve_mode_ugid_time;
		}

		while ((d = readdir(dp))) {
			char *new_source, *new_dest;

			new_source = concat_subpath_file(source, d->d_name);
			if (!new_source)
				continue;
			new_dest = concat_path_file(dest, d->d_name);
			if (copy_dir_1(inode_map, new_source, new_dest) < 0)
				retval = -1;
			free(new_source);
			free(new_dest);
		}
		closedir(dp);

		if (!dest_exists &&
		    chmod(dest, source_stat.st_mode & ~saved_umask) < 0) {
			error_errno(_("can't preserve permissions of '%s'"), dest);
			/* retval = -1; - WRONG! copy *WAS* made */
		}
		goto preserve_mode_ugid_time;
	}

	if (S_ISREG(source_stat.st_mode)) { /* "cp [-opts] regular_file thing2" */
		int src_fd;
		int dst_fd;
		mode_t new_mode;

		if (S_ISLNK(source_stat.st_mode)) {
			/* "cp -d symlink dst": create a link */
			goto dont_cat;
		}

		if (1 /*ENABLE_FEATURE_PRESERVE_HARDLINKS*/) {
			const char *link_target;
			link_target = is_in_ino_dev_hashtable(inode_map, &source_stat);
			if (link_target) {
				if (link(link_target, dest) < 0) {
					ovr = do_unlink(dest);
					if (ovr < 0)
						return ovr;
					if (link(link_target, dest) < 0)
						return error_errno(_("can't create link '%s'"), dest);
				}
				return 0;
			}
			add_to_ino_dev_hashtable(inode_map, &source_stat, dest);
		}

		src_fd = open(source, O_RDONLY);
		if (src_fd < 0)
			return error_errno(_("can't open '%s'"), source);

		/* Do not try to open with weird mode fields */
		new_mode = source_stat.st_mode;
		if (!S_ISREG(source_stat.st_mode))
			new_mode = 0666;

		dst_fd = open(dest, O_WRONLY|O_CREAT|O_EXCL, new_mode);
		if (dst_fd == -1) {
			ovr = do_unlink(dest);
			if (ovr < 0) {
				close(src_fd);
				return ovr;
			}
			/* It shouldn't exist. If it exists, do not open (symlink attack?) */
			dst_fd = open(dest, O_WRONLY|O_CREAT|O_EXCL, new_mode);
			if (dst_fd < 0) {
				close(src_fd);
				return error_errno(_("can't open '%s'"), dest);
			}
		}

		switch (copy_fd(src_fd, dst_fd)) {
		case COPY_READ_ERROR:
			error(_("copy-fd: read returned %s"), strerror(errno));
			retval = -1;
			break;
		case COPY_WRITE_ERROR:
			error(_("copy-fd: write returned %s"), strerror(errno));
			retval = -1;
			break;
		}

		/* Careful with writing... */
		if (close(dst_fd) < 0)
			retval = error_errno(_("error writing to '%s'"), dest);
		/* ...but read size is already checked by bb_copyfd_eof */
		close(src_fd);
		/*
		 * "cp /dev/something new_file" should not
		 * copy mode of /dev/something
		 */
		if (!S_ISREG(source_stat.st_mode))
			return retval;
		goto preserve_mode_ugid_time;
	}
dont_cat:

	/* Source is a symlink or a special file */
	/* We are lazy here, a bit lax with races... */
	if (dest_exists) {
		errno = EEXIST;
		ovr = do_unlink(dest);
		if (ovr < 0)
			return ovr;
	}
	if (S_ISLNK(source_stat.st_mode)) {
		struct strbuf lpath = STRBUF_INIT;
		if (!strbuf_readlink(&lpath, source, 0)) {
			int r = symlink(lpath.buf, dest);
			strbuf_release(&lpath);
			if (r < 0)
				return error_errno(_("can't create symlink '%s'"), dest);
			if (lchown(dest, source_stat.st_uid, source_stat.st_gid) < 0)
				error_errno(_("can't preserve %s of '%s'"), "ownership", dest);
		} else {
			/* EINVAL => "file: Invalid argument" => puzzled user */
			const char *errmsg = _("not a symlink");
			int err = errno;

			if (err != EINVAL)
				errmsg = strerror(err);
			error(_("%s: cannot read link: %s"), source, errmsg);
			strbuf_release(&lpath);
		}
		/*
		 * _Not_ jumping to preserve_mode_ugid_time: symlinks
		 * don't have those
		 */
		return 0;
	}
	if (S_ISBLK(source_stat.st_mode) ||
	    S_ISCHR(source_stat.st_mode) ||
	    S_ISSOCK(source_stat.st_mode) ||
	    S_ISFIFO(source_stat.st_mode)) {
		if (mknod(dest, source_stat.st_mode, source_stat.st_rdev) < 0)
			return error_errno(_("can't create '%s'"), dest);
	} else
		return error(_("unrecognized file '%s' with mode %x"),
			     source, source_stat.st_mode);

preserve_mode_ugid_time:

	if (1 /*FILEUTILS_PRESERVE_STATUS*/) {
		struct timeval times[2];

		times[1].tv_sec = times[0].tv_sec = source_stat.st_mtime;
		times[1].tv_usec = times[0].tv_usec = 0;
		/* BTW, utimes sets usec-precision time - just FYI */
		if (utimes(dest, times) < 0)
			error_errno(_("can't preserve %s of '%s'"), "times", dest);
		if (chown(dest, source_stat.st_uid, source_stat.st_gid) < 0) {
			source_stat.st_mode &= ~(S_ISUID | S_ISGID);
			error_errno(_("can't preserve %s of '%s'"), "ownership", dest);
		}
		if (chmod(dest, source_stat.st_mode) < 0)
			error_errno(_("can't preserve %s of '%s'"), "permissions", dest);
	}

	return retval;
}

/*
 * Return:
 * -1 error, copy not made
 *  0 copy is made
 *
 * Failures to preserve mode/owner/times are not reported in exit
 * code. No support for preserving SELinux security context. Symlinks
 * and hardlinks are preserved.
 */
int copy_dir_recursively(const char *source, const char *dest)
{
	int ret;
	struct hashmap inode_map;

	hashmap_init(&inode_map, inode_cmp, 1024);
	ret = copy_dir_1(&inode_map, source, dest);
	hashmap_free(&inode_map, 1);
	return ret;
}
