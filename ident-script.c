#include "cache.h"
#include "ident-script.h"
#include "quote.h"

void ident_script_init(struct ident_script *ident)
{
	ident->name = NULL;
	ident->email = NULL;
	ident->date = NULL;
}

void ident_script_release(struct ident_script *ident)
{
	free(ident->name);
	free(ident->email);
	free(ident->date);
	ident_script_init(ident);
}

/**
 * Reads a KEY=VALUE shell variable assignment from `fp`, returning the VALUE
 * as a newly-allocated string. VALUE must be a quoted string, and the KEY must
 * match `key`. Returns NULL on failure.
 *
 * This is used by read_author_script() to read the GIT_AUTHOR_* variables from
 * the author-script.
 */
static char *read_shell_var(FILE *fp, const char *key)
{
	struct strbuf sb = STRBUF_INIT;
	const char *str;

	if (strbuf_getline_lf(&sb, fp))
		goto fail;

	if (!skip_prefix(sb.buf, key, &str))
		goto fail;

	if (!skip_prefix(str, "=", &str))
		goto fail;

	strbuf_remove(&sb, 0, str - sb.buf);

	str = sq_dequote(sb.buf);
	if (!str)
		goto fail;

	return strbuf_detach(&sb, NULL);

fail:
	strbuf_release(&sb);
	return NULL;
}

int read_author_script(struct ident_script *ident, const char *filename)
{
	FILE *fp;

	fp = fopen(filename, "r");
	if (!fp) {
		if (errno == ENOENT)
			return 0;
		die_errno(_("could not open '%s' for reading"), filename);
	}

	free(ident->name);
	ident->name = read_shell_var(fp, "GIT_AUTHOR_NAME");
	if (!ident->name) {
		fclose(fp);
		return -1;
	}

	free(ident->email);
	ident->email = read_shell_var(fp, "GIT_AUTHOR_EMAIL");
	if (!ident->email) {
		fclose(fp);
		return -1;
	}

	free(ident->date);
	ident->date = read_shell_var(fp, "GIT_AUTHOR_DATE");
	if (!ident->date) {
		fclose(fp);
		return -1;
	}

	if (fgetc(fp) != EOF) {
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return 0;
}

void write_author_script(const struct ident_script *ident, const char *filename)
{
	struct strbuf sb = STRBUF_INIT;

	strbuf_addstr(&sb, "GIT_AUTHOR_NAME=");
	sq_quote_buf(&sb, ident->name);
	strbuf_addch(&sb, '\n');

	strbuf_addstr(&sb, "GIT_AUTHOR_EMAIL=");
	sq_quote_buf(&sb, ident->email);
	strbuf_addch(&sb, '\n');

	strbuf_addstr(&sb, "GIT_AUTHOR_DATE=");
	sq_quote_buf(&sb, ident->date);
	strbuf_addch(&sb, '\n');

	write_file(filename, "%s", sb.buf);
	strbuf_release(&sb);
}
