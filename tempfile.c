#include "tempfile.h"

void trim_last_path_elm(struct strbuf *path)
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

void resolve_symlink(struct strbuf *path)
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
