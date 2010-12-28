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
	printf("M %06o :%d ", mode, mark);
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
		snprintf(gitsvnline, MAX_GITSVN_LINE_LEN, "\n\ngit-svn-id: %s@%d %s\n",
				 pool_fetch(url), revision, pool_fetch(uuid));
	} else {
		*gitsvnline = '\0';
	}
	printf("commit refs/heads/master\n");
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

	printf("progress Imported commit %d.\n\n", revision);
}

static void die_short_read(void)
{
	if (buffer_ferror())
		die_errno("error reading dump file");
	die("invalid dump: unexpected end of file");
}

void fast_export_blob(uint32_t mode, uint32_t mark, uint32_t len)
{
	if (mode == REPO_MODE_LNK) {
		/* svn symlink blobs start with "link " */
		len -= 5;
		if (buffer_skip_bytes(5) != 5)
			die_short_read();
	}
	printf("blob\nmark :%d\ndata %d\n", mark, len);
	if (buffer_copy_bytes(len) != len)
		die_short_read();
	fputc('\n', stdout);
}
