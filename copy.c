#include "cache.h"

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

#if 0
/* Return:
 * -1 error, copy not made
 *  0 copy is made or user answered "no" in interactive mode
 *    (failures to preserve mode/owner/times are not reported in exit code)
 */
int FAST_FUNC copy_file(const char *source, const char *dest, int flags)
{
	/* This is a recursive function, try to minimize stack usage */
	/* NB: each struct stat is ~100 bytes */
	struct stat source_stat;
	struct stat dest_stat;
	smallint retval = 0;
	smallint dest_exists = 0;
	smallint ovr;

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
		tp = is_in_ino_dev_hashtable(&source_stat);
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
		add_to_ino_dev_hashtable(&dest_stat, NULL);

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
			if (copy_file(new_source, new_dest, flags & ~FILEUTILS_DEREFERENCE_L0) < 0)
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

		if (ENABLE_FEATURE_PRESERVE_HARDLINKS) {
			const char *link_target;
			link_target = is_in_ino_dev_hashtable(&source_stat);
			if (link_target) {
				if (link(link_target, dest) < 0) {
					ovr = ask_and_unlink(dest, flags);
					if (ovr <= 0)
						return ovr;
					if (link(link_target, dest) < 0)
						return error_errno(_("can't create link '%s'"), dest);
				}
				return 0;
			}
			add_to_ino_dev_hashtable(&source_stat, dest);
		}

		src_fd = open_or_warn(source, O_RDONLY);
		if (src_fd < 0)
			return -1;

		/* Do not try to open with weird mode fields */
		new_mode = source_stat.st_mode;
		if (!S_ISREG(source_stat.st_mode))
			new_mode = 0666;

		/* POSIX way is a security problem versus (sym)link attacks */
		if (!ENABLE_FEATURE_NON_POSIX_CP) {
			dst_fd = open(dest, O_WRONLY|O_CREAT|O_TRUNC, new_mode);
		} else { /* safe way: */
			dst_fd = open(dest, O_WRONLY|O_CREAT|O_EXCL, new_mode);
		}
		if (dst_fd == -1) {
			ovr = ask_and_unlink(dest, flags);
			if (ovr <= 0) {
				close(src_fd);
				return ovr;
			}
			/* It shouldn't exist. If it exists, do not open (symlink attack?) */
			dst_fd = open3_or_warn(dest, O_WRONLY|O_CREAT|O_EXCL, new_mode);
			if (dst_fd < 0) {
				close(src_fd);
				return -1;
			}
		}

		if (bb_copyfd_eof(src_fd, dst_fd) == -1)
			retval = -1;
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
		ovr = ask_and_unlink(dest, flags);
		if (ovr <= 0)
			return ovr;
	}
	if (S_ISLNK(source_stat.st_mode)) {
		char *lpath = xmalloc_readlink_or_warn(source);
		if (lpath) {
			int r = symlink(lpath, dest);
			free(lpath);
			if (r < 0)
				return error_errno(_("can't create symlink '%s'"), dest);
			if (lchown(dest, source_stat.st_uid, source_stat.st_gid) < 0)
				error_errno(_("can't preserve %s of '%s'"), "ownership", dest);
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
#endif
