#include "cache.h"
#include "rebase-common.h"

void rebase_options_init(struct rebase_options *opts)
{
	oidclr(&opts->onto);
	opts->onto_name = NULL;

	oidclr(&opts->upstream);

	oidclr(&opts->orig_head);
	opts->orig_refname = NULL;

	opts->resolvemsg = NULL;
}

void rebase_options_release(struct rebase_options *opts)
{
	free(opts->onto_name);
	free(opts->orig_refname);
}

void rebase_options_swap(struct rebase_options *dst, struct rebase_options *src)
{
	struct rebase_options tmp = *dst;
	*dst = *src;
	*src = tmp;
}

static int state_file_exists(const char *dir, const char *file)
{
	return file_exists(mkpath("%s/%s", dir, file));
}

static int read_state_file(struct strbuf *sb, const char *dir, const char *file)
{
	const char *path = mkpath("%s/%s", dir, file);
	strbuf_reset(sb);
	if (strbuf_read_file(sb, path, 0) >= 0)
		return sb->len;
	else
		return error(_("could not read '%s'"), path);
}

int rebase_options_load(struct rebase_options *opts, const char *dir)
{
	struct strbuf sb = STRBUF_INIT;
	const char *filename;

	/* opts->orig_refname */
	if (read_state_file(&sb, dir, "head-name") < 0)
		return -1;
	strbuf_trim(&sb);
	if (starts_with(sb.buf, "refs/heads/"))
		opts->orig_refname = strbuf_detach(&sb, NULL);
	else if (!strcmp(sb.buf, "detached HEAD"))
		opts->orig_refname = NULL;
	else
		return error(_("could not parse %s"), mkpath("%s/%s", dir, "head-name"));

	/* opts->onto */
	if (read_state_file(&sb, dir, "onto") < 0)
		return -1;
	strbuf_trim(&sb);
	if (get_oid_hex(sb.buf, &opts->onto) < 0)
		return error(_("could not parse %s"), mkpath("%s/%s", dir, "onto"));

	/*
	 * We always write to orig-head, but interactive rebase used to write
	 * to head. Fall back to reading from head to cover for the case that
	 * the user upgraded git with an ongoing interactive rebase.
	 */
	filename = state_file_exists(dir, "orig-head") ? "orig-head" : "head";
	if (read_state_file(&sb, dir, filename) < 0)
		return -1;
	strbuf_trim(&sb);
	if (get_oid_hex(sb.buf, &opts->orig_head) < 0)
		return error(_("could not parse %s"), mkpath("%s/%s", dir, filename));

	strbuf_release(&sb);
	return 0;
}

static int write_state_text(const char *dir, const char *file, const char *string)
{
	return write_file(mkpath("%s/%s", dir, file), "%s", string);
}

void rebase_options_save(const struct rebase_options *opts, const char *dir)
{
	const char *head_name = opts->orig_refname;
	if (!head_name)
		head_name = "detached HEAD";
	write_state_text(dir, "head-name", head_name);
	write_state_text(dir, "onto", oid_to_hex(&opts->onto));
	write_state_text(dir, "orig-head", oid_to_hex(&opts->orig_head));
}
