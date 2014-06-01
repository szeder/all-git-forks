#include "tempfile.h"
#include "sigchain.h"
#include "cache.h"

static struct temp_file *volatile temp_file_list;

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

int temp_file(struct temp_file *tf, const char *path, const char *dest, int flags)
{
	if (!temp_file_list) {
		/* One-time initialization */
		sigchain_push_common(remove_temp_file_on_signal);
		atexit(remove_temp_file);
	}

	assert(!tf->active);

	if (!tf->on_list) {
		/* Initialize *tf and add it to temp_file_list: */
		tf->fd = -1;
		tf->active = 0;
		tf->owner = 0;
		tf->on_list = 1;
		strbuf_init(&tf->filename, 0);
		strbuf_init(&tf->destination, 0);
		tf->next = temp_file_list;
		temp_file_list = tf;
	}

	strbuf_addstr(&tf->filename, path);
	strbuf_addstr(&tf->destination, dest);
	if (!(flags & TEMP_NODEREF))
		resolve_symlink(&tf->destination);

	tf->fd = open(tf->filename.buf, O_RDWR | O_CREAT | O_EXCL, 0666);
	if (tf->fd < 0) {
		strbuf_reset(&tf->filename);
		strbuf_reset(&tf->destination);
		return -1;
	}
	tf->owner = getpid();
	tf->active = 1;
	if (adjust_shared_perm(tf->filename.buf)) {
		error("cannot fix permission bits on %s", tf->filename.buf);
		rollback_temp_file(tf);
		return -1;
	}
	return tf->fd;
}

void rollback_temp_file(struct temp_file *temp_file)
{
	if (temp_file->active) {
		if (temp_file->fd >= 0)
			close_temp_file(temp_file);
		unlink_or_warn(temp_file->filename.buf);
		temp_file->active = 0;
		strbuf_reset(&temp_file->filename);
		strbuf_reset(&temp_file->destination);
	}
}

int commit_temp_file(struct temp_file *temp_file)
{
	if (temp_file->fd >= 0 && close_temp_file(temp_file))
		return -1;

	if (!temp_file->active)
		die("BUG: attempt to commit unlocked object");

	if (rename(temp_file->filename.buf, temp_file->destination.buf))
		return -1;

	temp_file->active = 0;
	strbuf_reset(&temp_file->filename);
	strbuf_reset(&temp_file->destination);
	return 0;
}

int close_temp_file(struct temp_file *temp_file)
{
	int fd = temp_file->fd;
	temp_file->fd = -1;
	return close(fd);
}
