/*
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "git-compat-util.h"
#include "strbuf.h"
#include "repo_tree.h"
#include "fast_export.h"

const char *repo_read_path(const char *path, uint32_t *mode_out)
{
	static struct strbuf buf = STRBUF_INIT;

	strbuf_reset(&buf);
	fast_export_ls(path, mode_out, &buf);
	return buf.buf;
}

void repo_copy(uint32_t revision, const char *src, const char *dst)
{
	uint32_t mode;
	struct strbuf data = STRBUF_INIT;

	fast_export_ls_rev(revision, src, &mode, &data);
	fast_export_modify(dst, mode, data.buf);
	strbuf_release(&data);
}

void repo_delete(const char *path)
{
	fast_export_delete(path);
}
