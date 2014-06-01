#include "tempfile.h"

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
