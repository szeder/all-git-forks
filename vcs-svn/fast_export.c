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

static uint32_t first_commit_done;
static struct line_buffer postimage = LINE_BUFFER_INIT;
static struct line_buffer report_buffer = LINE_BUFFER_INIT;

void fast_export_init(int fd)
{
	if (buffer_fdinit(&report_buffer, fd))
		die_errno("cannot read from file descriptor %d", fd);
	if (buffer_tmpfile_init(&postimage))
		die_errno("cannot write temporary file for delta application");
}

void fast_export_deinit(void)
{
	if (buffer_deinit(&report_buffer))
		die_errno("error closing fast-import feedback stream");
	if (buffer_deinit(&postimage))
		die_errno("error removing temporary file for delta application");
}

void fast_export_reset(void)
{
	buffer_reset(&report_buffer);
	buffer_reset(&postimage);
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

static void ls_from_rev(uint32_t rev, const uint32_t *path)
{
	/* ls :5 path/to/old/file */
	printf("ls :%"PRIu32" ", rev);
	pool_print_seq(REPO_MAX_PATH_DEPTH, path, '/', stdout);
	putchar('\n');
	fflush(stdout);
}

static int ends_with(const char *s, size_t len, const char *suffix)
{
	const size_t suffixlen = strlen(suffix);
	if (len < suffixlen)
		return 0;
	return !memcmp(s + len - suffixlen, suffix, suffixlen);
}

static int parse_ls_response_line(const char *line, struct strbuf *objnam)
{
	const char *end = line + strlen(line);
	const char *name, *tab;

	if (end - line < strlen("100644 blob "))
		return error("ls response too short: %s", line);
	name = line + strlen("100644 blob ");
	tab = memchr(name, '\t', end - name);
	if (!tab)
		return error("ls response does not contain tab: %s", line);
	strbuf_add(objnam, name, tab - name);
	return 0;
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

static off_t cat_from_rev(uint32_t rev, const uint32_t *path)
{
	const char *response;
	off_t length = length;
	struct strbuf blob_name = STRBUF_INIT;

	ls_from_rev(rev, path);
	response = get_response_line();
	if (parse_ls_response_line(response, &blob_name))
		die("invalid ls response: %s", response);

	printf("cat-blob %s\n", blob_name.buf);
	fflush(stdout);
	response = get_response_line();
	if (parse_cat_response_line(response, &length))
		die("invalid cat-blob response: %s", response);
	strbuf_release(&blob_name);
	return length;
}

static long apply_delta(off_t len, struct line_buffer *input,
			off_t preimage_len, uint32_t old_mode)
{
	long ret;
	struct sliding_view preimage = SLIDING_VIEW_INIT(&report_buffer);
	FILE *out;

	out = buffer_tmpfile_rewind(&postimage);
	if (!out)
		die("cannot open temporary file for blob retrieval");
	if (old_mode == REPO_MODE_LNK) {
		strbuf_addstr(&preimage.buf, "link ");
		if (preimage_len >= 0)
			preimage_len += strlen("link ");
	}
	if (svndiff0_apply(input, len, &preimage, out))
		die("cannot apply delta");
	if (preimage_len >= 0) {
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

static void record_postimage(uint32_t mark, uint32_t mode,
				long postimage_len)
{
	if (mode == REPO_MODE_LNK) {
		buffer_skip_bytes(&postimage, strlen("link "));
		postimage_len -= strlen("link ");
	}
	printf("blob\nmark :%"PRIu32"\ndata %ld\n", mark, postimage_len);
	buffer_copy_bytes(&postimage, postimage_len);
	fputc('\n', stdout);
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
	postimage_len = apply_delta((off_t) len, input,
						old_mark ? cat_mark(old_mark) : -1,
						old_mode);
	record_postimage(mark, mode, postimage_len);
}

void fast_export_blob_delta_rev(uint32_t mode, uint32_t mark,
				uint32_t old_mode, uint32_t old_rev,
				const uint32_t *old_path, uint32_t len,
				struct line_buffer *input)
{
	long postimage_len;
	if (len > maximum_signed_value_of_type(off_t))
		die("enormous delta");
	postimage_len = apply_delta((off_t) len, input,
						cat_from_rev(old_rev, old_path),
						old_mode);
	record_postimage(mark, mode, postimage_len);
}
