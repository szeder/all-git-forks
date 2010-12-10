/*
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "git-compat-util.h"
#include "strbuf.h"
#include "fast_export.h"
#include "repo_tree.h"
#include "svndiff.h"
#include "sliding_window.h"
#include "line_buffer.h"
#include "string_pool.h"

#define MAX_GITSVN_LINE_LEN 4096
#define REPORT_FILENO 3

static uint32_t first_commit_done;
static struct line_buffer postimage = LINE_BUFFER_INIT;
static struct line_buffer report_buffer = LINE_BUFFER_INIT;

/* NEEDSWORK: move to fast_export_init() */
static int init_postimage(void)
{
	static int postimage_initialized;
	if (postimage_initialized)
		return 0;
	postimage_initialized = 1;
	return buffer_tmpfile_init(&postimage);
}

static int init_report_buffer(int fd)
{
	static int report_buffer_initialized;
	if (report_buffer_initialized)
		return 0;
	report_buffer_initialized = 1;
	return buffer_fdinit(&report_buffer, fd);
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

static char gitsvnline[MAX_GITSVN_LINE_LEN];
void fast_export_commit(uint32_t revision, uint32_t author, char *log,
			uint32_t uuid, uint32_t url,
			unsigned long timestamp)
{
	if (!log)
		log = "";
	if (~uuid && ~url) {
		snprintf(gitsvnline, MAX_GITSVN_LINE_LEN,
				"\n\ngit-svn-id: %s@%"PRIu32" %s\n",
				 pool_fetch(url), revision, pool_fetch(uuid));
	} else {
		*gitsvnline = '\0';
	}
	printf("commit refs/heads/master\n");
	printf("mark :%"PRIu32"\n", revision);
	printf("committer %s <%s@%s> %ld +0000\n",
		   ~author ? pool_fetch(author) : "nobody",
		   ~author ? pool_fetch(author) : "nobody",
		   ~uuid ? pool_fetch(uuid) : "local", timestamp);
	printf("data %"PRIu32"\n%s%s\n",
		   (uint32_t) (strlen(log) + strlen(gitsvnline)),
		   log, gitsvnline);
	if (!first_commit_done) {
		if (revision > 1)
			printf("from refs/heads/master^0\n");
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

static int ends_with(const char *s, size_t len, const char *suffix)
{
	const size_t suffixlen = strlen(suffix);
	if (len < suffixlen)
		return 0;
	return !memcmp(s + len - suffixlen, suffix, suffixlen);
}

static int parse_cat_response_line(const char *header, off_t *len)
{
	size_t headerlen = strlen(header);
	const char *type;
	const char *end;

	if (ends_with(header, headerlen, " missing"))
		return error("cat-blob reports missing blob: %s", header);
	type = memmem(header, headerlen, " blob ", strlen(" blob "));
	if (!type)
		return error("cat-blob header has wrong object type: %s", header);
	*len = strtoumax(type + strlen(" blob "), (char **) &end, 10);
	if (end == type + strlen(" blob "))
		return error("cat-blob header does not contain length: %s", header);
	if (*end)
		return error("cat-blob header contains garbage after length: %s", header);
	return 0;
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

static off_t cat_mark(uint32_t mark)
{
	const char *response;
	off_t length = length;

	printf("cat-blob :%"PRIu32"\n", mark);
	fflush(stdout);
	response = get_response_line();
	if (parse_cat_response_line(response, &length))
		die("invalid cat-blob response: %s", response);
	return length;
}

static long apply_delta(uint32_t mark, off_t len, struct line_buffer *input,
			uint32_t old_mark, uint32_t old_mode)
{
	long ret;
	off_t preimage_len = 0;
	struct sliding_view preimage = SLIDING_VIEW_INIT(&report_buffer);
	FILE *out;

	if (init_postimage() || !(out = buffer_tmpfile_rewind(&postimage)))
		die("cannot open temporary file for blob retrieval");
	if (init_report_buffer(REPORT_FILENO))
		die("cannot open fd 3 for feedback from fast-import");
	if (old_mark)
		preimage_len = cat_mark(old_mark);
	if (old_mode == REPO_MODE_LNK) {
		strbuf_addstr(&preimage.buf, "link ");
		preimage_len += strlen("link ");
	}
	if (svndiff0_apply(input, len, &preimage, out))
		die("cannot apply delta");
	if (old_mark) {
		/* Read the remainder of preimage and trailing newline. */
		if (move_window(&preimage, preimage_len, 1))
			die("cannot seek to end of input");
		if (preimage.buf.buf[0] != '\n')
			die("missing newline after cat-blob response");
	}
	ret = buffer_tmpfile_prepare_to_read(&postimage);
	if (ret < 0)
		die("cannot read temporary file for blob retrieval");
	strbuf_release(&preimage.buf);
	return ret;
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

void fast_export_blob_delta(uint32_t mode, uint32_t mark,
				uint32_t old_mode, uint32_t old_mark,
				uint32_t len, struct line_buffer *input)
{
	long postimage_len;
	if (len > maximum_signed_value_of_type(off_t))
		die("enormous delta");
	postimage_len = apply_delta(mark, (off_t) len, input, old_mark, old_mode);
	if (mode == REPO_MODE_LNK) {
		buffer_skip_bytes(&postimage, strlen("link "));
		postimage_len -= strlen("link ");
	}
	printf("blob\nmark :%"PRIu32"\ndata %ld\n", mark, postimage_len);
	buffer_copy_bytes(&postimage, postimage_len);
	fputc('\n', stdout);
}
