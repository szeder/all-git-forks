/*
 * test-run-command.c: test run command API.
 *
 * (C) 2009 Ilari Liusvaara <ilari.liusvaara@elisanet.fi>
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "git-compat-util.h"
#include "run-command.h"
#include "strbuf.h"
#include <string.h>
#include <errno.h>

static int run_write_async(int in, int out, void *data)
{
	const char *msg = data;

	FILE *fout = xfdopen(out, "w");
	fprintf(fout, "%s\n", msg);

	fclose(fout);
	return 0;
}

int main(int argc, char **argv)
{
	struct child_process proc;
	struct async write_out;
	struct async write_err;

	memset(&proc, 0, sizeof(proc));

	if (argc < 2)
		return 1;

	if (!strcmp(argv[1], "start-command-ENOENT")) {
		if (argc < 3)
			return 1;
		proc.argv = (const char **)argv+2;

		if (start_command(&proc) < 0 && errno == ENOENT)
			return 0;
		fprintf(stderr, "FAIL %s\n", argv[1]);
		return 1;
	}

	if (!strcmp(argv[1], "out2strbuf")) {
#ifndef NO_PTHREADS
		struct strbuf output = STRBUF_INIT;

		memset(&write_out, 0, sizeof(write_out));
		write_out.data = "Hallo Stdout";
		write_out.proc = run_write_async;
		write_out.out = -1;

		memset(&write_err, 0, sizeof(write_err));
		write_err.data = "Hallo Stderr";
		write_err.proc = run_write_async;
		write_err.out = -1;

		start_async(&write_out);
		start_async(&write_err);

		read_2_fds_into_strbuf(write_out.out, write_err.out, &output);

		finish_async(&write_out);
		finish_async(&write_err);

		printf("%s", output.buf);
		strbuf_release(&output);
#endif /* NO_PTHREADS */

		return 0;
	}

	fprintf(stderr, "check usage\n");
	return 1;
}
