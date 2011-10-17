/*
 * Copyright (c) 2011, Google Inc.
 */
#include "cache.h"
#include "run-command.h"
#include "strbuf.h"
#include "gpg-interface.h"
#include "sigchain.h"

static char *configured_signing_key;

void set_signing_key(const char *key)
{
	free(configured_signing_key);
	configured_signing_key = xstrdup(key);
}

int git_gpg_config(const char *var, const char *value, void *cb)
{
	if (!strcmp(var, "user.signingkey")) {
		if (!value)
			return config_error_nonbool(var);
		set_signing_key(value);
	}
	return 0;
}

const char *get_signing_key(void)
{
	if (configured_signing_key)
		return configured_signing_key;
	return git_committer_info(IDENT_ERROR_ON_NO_NAME|IDENT_NO_DATE);
}

int sign_buffer(struct strbuf *buffer, const char *signing_key)
{
	struct child_process gpg;
	const char *args[4];
	ssize_t len;
	int i, j;

	memset(&gpg, 0, sizeof(gpg));
	gpg.argv = args;
	gpg.in = -1;
	gpg.out = -1;
	args[0] = "gpg";
	args[1] = "-bsau";
	args[2] = signing_key;
	args[3] = NULL;

	if (start_command(&gpg))
		return error(_("could not run gpg."));

	/*
	 * When the username signingkey is bad, program could be terminated
	 * because gpg exits without reading and then write gets SIGPIPE.
	 */
	sigchain_push(SIGPIPE, SIG_IGN);

	if (write_in_full(gpg.in, buffer->buf, buffer->len) != buffer->len) {
		close(gpg.in);
		close(gpg.out);
		finish_command(&gpg);
		return error(_("gpg did not accept the data"));
	}
	close(gpg.in);
	len = strbuf_read(buffer, gpg.out, 1024);
	close(gpg.out);

	sigchain_pop(SIGPIPE);

	if (finish_command(&gpg) || !len || len < 0)
		return error(_("gpg failed to sign the data"));

	/* Strip CR from the line endings, in case we are on Windows. */
	for (i = j = 0; i < buffer->len; i++)
		if (buffer->buf[i] != '\r') {
			if (i != j)
				buffer->buf[j] = buffer->buf[i];
			j++;
		}
	strbuf_setlen(buffer, j);

	return 0;
}

int verify_signed_buffer(const char *buf, size_t total, size_t payload)
{
	struct child_process gpg;
	const char *args_gpg[] = {"gpg", "--verify", "FILE", "-", NULL};
	char path[PATH_MAX];
	int fd, ret;

	fd = git_mkstemp(path, PATH_MAX, ".git_vtag_tmpXXXXXX");
	if (fd < 0)
		return error("could not create temporary file '%s': %s",
			     path, strerror(errno));
	if (write_in_full(fd, buf, total) < 0)
		return error("failed writing temporary file '%s': %s",
			     path, strerror(errno));
	close(fd);

	memset(&gpg, 0, sizeof(gpg));
	gpg.argv = args_gpg;
	gpg.in = -1;
	args_gpg[2] = path;
	if (start_command(&gpg)) {
		unlink(path);
		return error("could not run gpg.");
	}

	write_in_full(gpg.in, buf, payload);
	close(gpg.in);
	ret = finish_command(&gpg);

	unlink_or_warn(path);

	return ret;
}
