#include "cache.h"
#include "run-command.h"

static int _skip_object_filter;

void set_skip_object_filter(int skip)
{
	_skip_object_filter = skip;
}

int skip_object_filter()
{
	return _skip_object_filter;
}

static int _run_object_filter(const char *name, const void *buffer, unsigned long size, const char *type, struct strbuf *res)
{
	struct child_process cmd;
	const char *args[4];
	size_t len;
	size_t n;
	char sizebuf[32];
	char tmpbuf[1024];
	const char *p;
	struct pollfd fd;
	int ret;

	if ((_skip_object_filter) || (access(git_path("hooks/%s", name), X_OK) < 0))
		return 1;

	sprintf(sizebuf, "%lu", size);

	memset(&cmd, 0, sizeof(cmd));
	cmd.argv = args;
	cmd.in = -1;
	cmd.out = -1;
	args[0] = git_path("hooks/%s", name);
	args[1] = type;
	args[2] = sizebuf;
	args[3] = NULL;

	if (start_command(&cmd))
		die(_("could not run object filter."));

	p = buffer;
	len = size;

	while(len > 0) {
		if ((n = xwrite(cmd.in, p, len > 1024 ? 1024 : len)) != (len > 1024 ? 1024 : len)) {
			close(cmd.in);
			close(cmd.out);
			finish_command(&cmd);
			die(_("object filter did not accept the data"));
		}

		len -= n;
		p += n;

		fd.fd = cmd.out;
		fd.events = POLLIN;
		fd.revents = 0;

		ret = poll(&fd, 1, 0);
		if(ret < 0)
			die(_("polling for object filter failed"));

		if(ret) {
			if((n = xread(cmd.out, tmpbuf, 1024)) < 0)
				die(_("failed to read data from object filter"));

			strbuf_add(res, tmpbuf, n);
		}
	}

	close(cmd.in);

	while((n = xread(cmd.out, tmpbuf, 1024)) > 0)
		strbuf_add(res, tmpbuf, n);
	if(n < 0)
		die(_("failed to read data from object filter"));

	close(cmd.out);

	if (finish_command(&cmd))
		die(_("object filter failed to run"));

	return 0;
}

void *run_object_filter(const char *name, const void *buffer, unsigned long *size, const char *type)
{
	struct strbuf nbuf = STRBUF_INIT;
	void *buf;
	size_t len;

	if (_run_object_filter(name, buffer, *size, type, &nbuf))
		return NULL;

	buf = strbuf_detach(&nbuf, &len);
	*size = len;

	return buf;
}

void run_object_filter_strbuf(const char *name, struct strbuf *buf, const char *type)
{
	struct strbuf nbuf = STRBUF_INIT;

	if (_run_object_filter(name, buf->buf, buf->len, type, &nbuf))
		return;

	strbuf_swap(buf, &nbuf);
	strbuf_release(&nbuf);
}

