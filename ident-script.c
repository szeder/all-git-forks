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
 */
static char *read_git_var(FILE *fp, const char *var1, const char *var2)
{
	struct strbuf sb = STRBUF_INIT;
	const char *str;

	if (strbuf_getline_lf(&sb, fp))
		goto fail;

	if (!skip_prefix(sb.buf, "GIT_", &str) ||
	    !skip_prefix(str, var1, &str) ||
	    !skip_prefix(str, "_", &str) ||
	    !skip_prefix(str, var2, &str) ||
	    !skip_prefix(str, "=", &str))
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

int read_ident_script(struct ident_script *ident, const char *filename, const char *var)
{
	FILE *fp = fopen(filename, "r");
	if (!fp)
		return error(_("could not open '%s' for reading"), filename);

	free(ident->name);
	ident->name = read_git_var(fp, var, "NAME");
	if (!ident->name) {
		fclose(fp);
		return -1;
	}

	free(ident->email);
	ident->email = read_git_var(fp, var, "EMAIL");
	if (!ident->email) {
		fclose(fp);
		return -1;
	}

	free(ident->date);
	ident->date = read_git_var(fp, var, "DATE");
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

void write_ident_script(const struct ident_script *ident, const char *filename, const char *var)
{
	struct strbuf sb = STRBUF_INIT;

	strbuf_addf(&sb, "GIT_%s_NAME=", var);
	sq_quote_buf(&sb, ident->name ? ident->name : "");
	strbuf_addch(&sb, '\n');

	strbuf_addf(&sb, "GIT_%s_EMAIL=", var);
	sq_quote_buf(&sb, ident->email ? ident->email : "");
	strbuf_addch(&sb, '\n');

	strbuf_addf(&sb, "GIT_%s_DATE=", var);
	sq_quote_buf(&sb, ident->date ? ident->date : "");
	strbuf_addch(&sb, '\n');

	write_file(filename, "%s", sb.buf);
	strbuf_release(&sb);
}

int ident_script_from_line(struct ident_script *ident, const char *line, size_t len)
{
	struct ident_split split;

	if (split_ident_line(&split, line, len) < 0)
		return -1;

	free(ident->name);
	ident->name = xmemdupz(split.name_begin, split.name_end - split.name_begin);

	free(ident->email);
	ident->email = xmemdupz(split.mail_begin, split.mail_end - split.mail_begin);

	free(ident->date);
	ident->date = xstrdup(show_ident_date(&split, DATE_MODE(NORMAL)));

	return 0;
}
