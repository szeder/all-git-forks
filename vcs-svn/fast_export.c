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

void fast_export_delete(uint32_t depth, const uint32_t *path)
{
	printf("D \"");
	pool_print_seq_q(depth, path, '/', stdout);
	printf("\"\n");
}

static void fast_export_truncate(uint32_t depth, const uint32_t *path, uint32_t mode)
{
	fast_export_modify(depth, path, mode, "inline");
	printf("data 0\n\n");
}

void fast_export_modify(uint32_t depth, const uint32_t *path, uint32_t mode,
			const char *dataref)
{
	/* Mode must be 100644, 100755, 120000, or 160000. */
	if (!dataref) {
		fast_export_truncate(depth, path, mode);
		return;
	}
	printf("M %06"PRIo32" %s \"", mode, dataref);
	pool_print_seq_q(depth, path, '/', stdout);
	printf("\"\n");
}

static char gitsvnline[MAX_GITSVN_LINE_LEN];
void fast_export_begin_commit(uint32_t revision, uint32_t author, char *log,
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
}

void fast_export_end_commit(uint32_t revision)
{
	printf("progress Imported commit %"PRIu32".\n\n", revision);
}

static void die_short_read(struct line_buffer *input)
{
	if (buffer_ferror(input))
		die_errno("error reading dump file");
	die("invalid dump: unexpected end of file");
}

static void ls_from_rev(uint32_t rev, uint32_t depth, const uint32_t *path)
{
	/* ls :5 path/to/old/file */
	printf("ls :%"PRIu32" \"", rev);
	pool_print_seq_q(depth, path, '/', stdout);
	printf("\"\n");
	fflush(stdout);
}

static void ls_from_active_commit(uint32_t depth, const uint32_t *path)
{
	/* ls "path/to/file" */
	printf("ls \"");
	pool_print_seq_q(depth, path, '/', stdout);
	printf("\"\n");
	fflush(stdout);
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

static off_t cat_dataref(const char *dataref)
{
	const char *response;
	off_t length = length;
	assert(dataref);

	printf("cat-blob %s\n", dataref);
	fflush(stdout);
	response = get_response_line();
	if (parse_cat_response_line(response, &length))
		die("invalid cat-blob response: %s", response);
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

static void record_postimage(uint32_t mode, long postimage_len)
{
	if (mode == REPO_MODE_LNK) {
		buffer_skip_bytes(&postimage, strlen("link "));
		postimage_len -= strlen("link ");
	}
	printf("data %ld\n", postimage_len);
	buffer_copy_bytes(&postimage, postimage_len);
	fputc('\n', stdout);
}

void fast_export_data(uint32_t mode, uint32_t len, struct line_buffer *input)
{
	if (mode == REPO_MODE_LNK) {
		/* svn symlink blobs start with "link " */
		len -= 5;
		if (buffer_skip_bytes(input, 5) != 5)
			die_short_read(input);
	}
	printf("data %"PRIu32"\n", len);
	if (buffer_copy_bytes(input, len) != len)
		die_short_read(input);
	fputc('\n', stdout);
}

void fast_export_delta(uint32_t mode, uint32_t depth, const uint32_t *path,
				uint32_t old_mode, const char *dataref,
				uint32_t len, struct line_buffer *input)
{
	off_t preimage_len;
	long postimage_len;
	if (len > maximum_signed_value_of_type(off_t))
		die("enormous delta");
	preimage_len = dataref ? cat_dataref(dataref) : -1;

	/* NEEDSWORK: Will deadlock with very long paths. */
	fast_export_modify(depth, path, mode, "inline");
	postimage_len = apply_delta((off_t) len, input, preimage_len, old_mode);
	record_postimage(mode, postimage_len);
}

static void parse_ls_response(const char *response, uint32_t *mode,
					struct strbuf *dataref)
{
	const char *tab;
	const char *response_end;

	assert(response);
	response_end = response + strlen(response);
	if (*response == 'm')	/* missing! */
		die("unexpected ls response: %s", response);

	/* Mode. */
	if (response_end - response < strlen("100644") ||
	    response[strlen("100644")] != ' ')
		die("invalid ls response: missing mode: %s", response);
	*mode = 0;
	for (; *response != ' '; response++) {
		char ch = *response;
		if (ch < '0' || ch > '7')
			die("invalid ls response: mode is not octal: %s", response);
		*mode *= 8;
		*mode += ch - '0';
	}

	/* ' blob ' or ' tree ' */
	if (response_end - response < strlen(" blob ") ||
	    (response[1] != 'b' && response[1] != 't'))
		die("unexpected ls response: not a tree or blob: %s", response);
	response += strlen(" blob ");

	/* Dataref. */
	tab = memchr(response, '\t', response_end - response);
	if (!tab)
		die("invalid ls response: missing tab: %s", response);
	strbuf_add(dataref, response, tab - response);
}

void fast_export_ls_rev(uint32_t rev, uint32_t depth, const uint32_t *path,
				uint32_t *mode, struct strbuf *dataref)
{
	ls_from_rev(rev, depth, path);
	parse_ls_response(get_response_line(), mode, dataref);
}

void fast_export_ls(uint32_t depth, const uint32_t *path,
				uint32_t *mode, struct strbuf *dataref)
{
	ls_from_active_commit(depth, path);
	parse_ls_response(get_response_line(), mode, dataref);
}
