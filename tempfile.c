#include "tempfile.h"
#include "cache.h"
#include "sigchain.h"

static struct temp_file *volatile temp_file_list;

/*
 * path = absolute or relative path name
 *
 * Remove the last path name element from path (leaving the preceding
 * "/", if any).  If path is empty or the root directory ("/"), set
 * path to the empty string.
 */
static void trim_last_path_elm(struct strbuf *path)
{
	int i = path->len;

	/* back up past trailing slashes, if any */
	while (i && path->buf[i - 1] == '/')
		i--;

	/*
	 * then go backwards until a slash, or the beginning of the
	 * string
	 */
	while (i && path->buf[i - 1] != '/')
		i--;

	strbuf_setlen(path, i);
}

/* We allow "recursive" symbolic links. Only within reason, though */
#define MAXDEPTH 5

/*
 * path contains a path that might be a symlink.
 *
 * If path is a symlink, attempt to overwrite it with a path to the
 * real file or directory (which may or may not exist), following a
 * chain of symlinks if necessary.  Otherwise, leave path unmodified.
 *
 * This is a best-effort routine.  If an error occurs, path will
 * either be left unmodified or will name a different symlink in a
 * symlink chain that started with the original path.
 */
static void resolve_symlink(struct strbuf *path)
{
	int depth = MAXDEPTH;
	static struct strbuf link = STRBUF_INIT;

	while (depth--) {
		if (strbuf_readlink(&link, path->buf, path->len) < 0)
			break;

		if (is_absolute_path(link.buf))
			/* absolute path simply replaces p */
			strbuf_reset(path);
		else
			/*
			 * link is a relative path, so replace the
			 * last element of p with it.
			 */
			trim_last_path_elm(path);

		strbuf_addbuf(path, &link);
	}
	strbuf_reset(&link);
}

static void remove_temp_file(void)
{
	pid_t me = getpid();

	while (temp_file_list) {
		if (temp_file_list->owner == me)
			rollback_temp_file(temp_file_list);
		temp_file_list = temp_file_list->next;
	}
}

static void remove_temp_file_on_signal(int signo)
{
	remove_temp_file();
	sigchain_pop(signo);
	raise(signo);
}

int initialize_temp_file(struct temp_file *tmp, const char *path, const char *dest, int flags)
{
	if (!temp_file_list) {
		/* One-time initialization */
		sigchain_push_common(remove_temp_file_on_signal);
		atexit(remove_temp_file);
	}

	/* A deactivated temp_file may not be reused */
	assert(!tmp->active);

	if (!tmp->on_list) {
		/* Initialize *tmp and add it to temp_file_list */
		tmp->fd = -1;
		tmp->active = 0;
		tmp->owner = 0;
		tmp->on_list = 1;
		strbuf_init(&tmp->filename, 0);
		strbuf_init(&tmp->destination, 0);
		tmp->next = temp_file_list;
		temp_file_list = tmp;
	}

	strbuf_addstr(&tmp->filename, path);
	strbuf_addstr(&tmp->destination, dest);
	if (!(flags & LOCK_NODEREF))
		resolve_symlink(&tmp->destination);

	tmp->fd = open(tmp->filename.buf, O_RDWR | O_CREAT | O_EXCL, 0666);
	if (tmp->fd < 0) {
		strbuf_reset(&tmp->filename);
		strbuf_reset(&tmp->destination);
		return -1;
	}
	tmp->owner = getpid();
	tmp->active = 1;
	if (adjust_shared_perm(tmp->filename.buf)) {
		error("cannot fix permission bits on %s", tmp->filename.buf);
		rollback_temp_file(tmp);
		return -1;
	}
	return tmp->fd;
}

int commit_temp_file(struct temp_file *tmp)
{
	/* Try closing the file. Return if unsuccessful. */
	if (tmp->fd >= 0 && close_temp_file(tmp))
		return -1;

	if (!tmp->active)
		die("BUG: attempt to rename deactivated tempfile");

	/* Try renaming the file. Return if unsuccessful. */
	if (rename(tmp->filename.buf, tmp->destination.buf))
		return -1;

	/* Deactivate tempfile */
	tmp->active = 0;
	strbuf_reset(&tmp->filename);
	strbuf_reset(&tmp->destination);
	return 0;
}

void rollback_temp_file(struct temp_file *tmp)
{
	if (tmp->active) {
		if (tmp->fd >= 0)
			close_temp_file(tmp);
		unlink_or_warn(tmp->filename.buf);
		tmp->active = 0;
		strbuf_reset(&tmp->filename);
		strbuf_reset(&tmp->destination);
	}
}

int close_temp_file(struct temp_file *tmp)
{
	int fd = tmp->fd;
	tmp->fd = -1;
	return close(fd);
}
