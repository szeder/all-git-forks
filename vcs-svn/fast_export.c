/*
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "git-compat-util.h"
#include "fast_export.h"
#include "line_buffer.h"
#include "repo_tree.h"
#include "string_pool.h"

#define MAX_GITSVN_LINE_LEN 4096

static uint32_t first_commit_done;
static struct line_buffer report_buffer = LINE_BUFFER_INIT;

void fast_export_init(int fd)
{
	if (buffer_fdinit(&report_buffer, fd))
		die_errno("cannot read from file descriptor %d", fd);
}

void fast_export_deinit(void)
{
	if (buffer_deinit(&report_buffer))
		die_errno("error closing fast-import feedback stream");
}

void fast_export_reset(void)
{
	buffer_reset(&report_buffer);
}

void fast_export_delete(uint32_t depth, uint32_t *path)
{
	putchar('D');
	putchar(' ');
	pool_print_seq(depth, path, '/', stdout);
	putchar('\n');
}

void fast_export_modify(uint32_t depth, uint32_t *path, uint32_t mode,
			uint32_t mark)
{
	/* Mode must be 100644, 100755, 120000, or 160000. */
	printf("M %06"PRIo32" :%"PRIu32" ", mode, mark);
	pool_print_seq(depth, path, '/', stdout);
	putchar('\n');
}

void fast_export_begin_commit(uint32_t revision)
{
	printf("# commit %"PRIu32".\n", revision);
}

static char gitsvnline[MAX_GITSVN_LINE_LEN];
void fast_export_commit(uint32_t revision, const char *author,
			const struct strbuf *log,
			const char *uuid, const char *url,
			unsigned long timestamp)
{
	static const struct strbuf empty = STRBUF_INIT;
	if (!log)
		log = &empty;
	if (*uuid && *url) {
		snprintf(gitsvnline, MAX_GITSVN_LINE_LEN,
				"\n\ngit-svn-id: %s@%"PRIu32" %s\n",
				 url, revision, uuid);
	} else {
		*gitsvnline = '\0';
	}
	printf("commit refs/heads/master\n");
	printf("mark :%"PRIu32"\n", revision);
	printf("committer %s <%s@%s> %ld +0000\n",
		   *author ? author : "nobody",
		   *author ? author : "nobody",
		   *uuid ? uuid : "local", timestamp);
	printf("data %"PRIuMAX"\n", log->len + strlen(gitsvnline));
	fwrite(log->buf, log->len, 1, stdout);
	printf("%s\n", gitsvnline);
	if (!first_commit_done) {
		if (revision > 1)
			printf("from :%"PRIu32"\n", revision - 1);
		first_commit_done = 1;
	}
	repo_diff(revision - 1, revision);
	fputc('\n', stdout);

	printf("progress Imported commit %"PRIu32".\n\n", revision);
}

static void die_short_read(struct line_buffer *input)
{
	if (buffer_ferror(input))
		die_errno("error reading dump file");
	die("invalid dump: unexpected end of file");
}

static const char *get_response_line(void)
{
	const char *line = buffer_read_line(&report_buffer);
	if (line)
		return line;
	if (buffer_ferror(&report_buffer))
		die_errno("error reading from fast-import");
	die("unexpected end of fast-import feedback");
}

void fast_export_blob(uint32_t mode, uint32_t mark, uint32_t len, struct line_buffer *input)
{
	if (mode == REPO_MODE_LNK) {
		/* svn symlink blobs start with "link " */
		len -= 5;
		if (buffer_skip_bytes(input, 5) != 5)
			die_short_read(input);
	}
	printf("blob\nmark :%"PRIu32"\ndata %"PRIu32"\n", mark, len);
	if (buffer_copy_bytes(input, len) != len)
		die_short_read(input);
	fputc('\n', stdout);
}
