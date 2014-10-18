#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vcs-svn/git-strbuf.h"

/*
 * git-isvn:
 *
 * This is a stripped down version of strbuf from git(1). As such it is GPL.
 * We will try and get rid of it eventually.
 */

/*
 * Used as the default ->buf value, so that people can always assume
 * buf is non NULL and ->buf is NUL terminated even for a freshly
 * initialized strbuf.
 */
char strbuf_slopbuf[1];

void strbuf_init(struct strbuf *sb, size_t hint)
{
	sb->alloc = sb->len = 0;
	sb->buf = strbuf_slopbuf;
	if (hint)
		strbuf_grow(sb, hint);
}

void strbuf_release(struct strbuf *sb)
{
	if (sb->alloc) {
		free(sb->buf);
		strbuf_init(sb, 0);
	}
}

void strbuf_grow(struct strbuf *sb, size_t extra)
{
	int new_buf = !sb->alloc;
#if 0
	if (unsigned_add_overflows(extra, 1) ||
	    unsigned_add_overflows(sb->len, extra + 1))
		die("you want to use way too much memory");
#endif
	if (new_buf)
		sb->buf = NULL;
	ALLOC_GROW(sb->buf, sb->len + extra + 1, sb->alloc);
	if (new_buf)
		sb->buf[0] = '\0';
}

static void strbuf_splice(struct strbuf *sb, size_t pos, size_t len,
				   const void *data, size_t dlen)
{
#if 0
	if (unsigned_add_overflows(pos, len))
		die("you want to use way too much memory");
#endif
	if (pos > sb->len) {
		printf("`pos' is too far after the end of the buffer\n");
		abort();
	}
	if (pos + len > sb->len) {
		printf("`pos + len' is too far after the end of the buffer\n");
		abort();
	}

	if (dlen >= len)
		strbuf_grow(sb, dlen - len);
	memmove(sb->buf + pos + dlen,
			sb->buf + pos + len,
			sb->len - pos - len);
	memcpy(sb->buf + pos, data, dlen);
	strbuf_setlen(sb, sb->len + dlen - len);
}

void strbuf_remove(struct strbuf *sb, size_t pos, size_t len)
{
	strbuf_splice(sb, pos, len, NULL, 0);
}

void strbuf_add(struct strbuf *sb, const void *data, size_t len)
{
	strbuf_grow(sb, len);
	memcpy(sb->buf + sb->len, data, len);
	strbuf_setlen(sb, sb->len + len);
}

size_t strbuf_fread(struct strbuf *sb, size_t size, FILE *f)
{
	size_t res;
	size_t oldalloc = sb->alloc;

	strbuf_grow(sb, size);
	res = fread(sb->buf + sb->len, 1, size, f);
	if (res > 0)
		strbuf_setlen(sb, sb->len + res);
	else if (oldalloc == 0)
		strbuf_release(sb);
	return res;
}
