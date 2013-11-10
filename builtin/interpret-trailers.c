/*
 * Builtin "git interpret-trailers"
 *
 * Copyright (c) 2013 Christian Couder <chriscool@tuxfamily.org>
 *
 */

#include "cache.h"
#include "builtin.h"
#include "parse-options.h"
#include "strbuf.h"

static const char * const git_interpret_trailers_usage[] = {
	N_("git interpret-trailers [--trim-empty] [--infile=file] [<token[=value]>...]"),
	NULL
};

static struct string_list trailer_list;

enum action_where { AFTER, MIDDLE, BEFORE };
enum action_if_exist { EXIST_ADD_IF_DIFFERENT, EXIST_ADD_IF_DIFFERENT_NEIGHBOR,
		       EXIST_ADD, EXIST_OVERWRITE, EXIST_DO_NOTHING };
enum action_if_missing { MISSING_DO_NOTHING, MISSING_ADD };

struct trailer_info {
	char *value;
	char *command;
	enum action_where where;
	enum action_if_exist if_exist;
	enum action_if_missing if_missing;
};

static int set_where(struct trailer_info *info, const char *value)
{
	if (!strcasecmp("after", value)) {
		info->where = AFTER;
	} else if (!strcasecmp("middle", value)) {
		info->where = MIDDLE;
	} else if (!strcasecmp("before", value)) {
		info->where = BEFORE;
	} else
		return 1;
	return 0;
}

static int set_if_exist(struct trailer_info *info, const char *value)
{
	if (!strcasecmp("add_if_different", value)) {
		info->if_exist = EXIST_ADD_IF_DIFFERENT;
	} else if (!strcasecmp("add_if_different_neighbor", value)) {
		info->if_exist = EXIST_ADD_IF_DIFFERENT_NEIGHBOR;
	} else if (!strcasecmp("add", value)) {
		info->if_exist = EXIST_ADD;
	} else if (!strcasecmp("overwrite", value)) {
		info->if_exist = EXIST_OVERWRITE;
	} else if (!strcasecmp("do_nothing", value)) {
		info->if_exist = EXIST_DO_NOTHING;
	} else
		return 1;
	return 0;
}

static int set_if_missing(struct trailer_info *info, const char *value)
{
	if (!strcasecmp("do_nothing", value)) {
		info->if_missing = MISSING_DO_NOTHING;
	} else if (!strcasecmp("add", value)) {
		info->if_missing = MISSING_ADD;
	} else
		return 1;
	return 0;
}

static int git_trailer_config(const char *key, const char *value, void *cb)
{
	if (!prefixcmp(key, "trailer.")) {
		const char *orig_key = key;
		char *name;
		struct string_list_item *item;
		struct trailer_info *info;
		enum { VALUE, COMMAND, WHERE, IF_EXIST, IF_MISSING } type;

		key += 8;
		if (!suffixcmp(key, ".value")) {
			name = xstrndup(key, strlen(key) - 6);
			type = VALUE;
		} else if (!suffixcmp(key, ".command")) {
			name = xstrndup(key, strlen(key) - 8);
			type = COMMAND;
		} else if (!suffixcmp(key, ".where")) {
			name = xstrndup(key, strlen(key) - 9);
			type = WHERE;
		} else if (!suffixcmp(key, ".if_exist")) {
			name = xstrndup(key, strlen(key) - 9);
			type = IF_EXIST;
		} else if (!suffixcmp(key, ".if_missing")) {
			name = xstrndup(key, strlen(key) - 11);
			type = IF_MISSING;
		} else
			return 0;

		item = string_list_insert(&trailer_list, name);

		if (!item->util)
			item->util = xcalloc(sizeof(struct trailer_info), 1);
		info = item->util;
		if (type == VALUE) {
			if (info->value)
				warning(_("more than one %s"), orig_key);
			info->value = xstrdup(value);
		} else if (type == COMMAND) {
			if (info->command)
				warning(_("more than one %s"), orig_key);
			info->command = xstrdup(value);
		} else if (type == WHERE) {
			if (set_where(info, value))
				warning(_("unknow value '%s' for key '%s'"), value, orig_key);
		} else if (type == IF_EXIST) {
			if (set_if_exist(info, value))
				warning(_("unknow value '%s' for key '%s'"), value, orig_key);
		} else if (type == IF_MISSING) {
			if (set_if_missing(info, value))
				warning(_("unknow value '%s' for key '%s'"), value, orig_key);
		} else {
			die("internal bug in git_trailer_config");
		}
	}
	return 0;
}

static void parse_trailer(struct strbuf *tok, struct strbuf *val, const char *trailer)
{
	char *end = strchr(trailer, '=');
	if (!end)
		end = strchr(trailer, ':');
	if (end) {
		strbuf_add(tok, trailer, end - trailer);
		strbuf_trim(tok);
		strbuf_addstr(val, end + 1);
		strbuf_trim(val);
	} else {
		strbuf_addstr(tok, trailer);
		strbuf_trim(tok);
	}
}

static void parse_trailer_into_string_lists(struct string_list *tok_list,
					    struct string_list *val_list,
					    const char *trailer)
{
	struct strbuf tok = STRBUF_INIT;
	struct strbuf val = STRBUF_INIT;
	parse_trailer(&tok, &val, trailer);
	string_list_append(tok_list, strbuf_detach(&tok, NULL));
	string_list_append(val_list, strbuf_detach(&val, NULL));
}

/* Get the length of buf from its beginning until its last alphanumeric character */
static size_t alnum_len(const char *buf, size_t len) {
	while (--len >= 0 && !isalnum(buf[len]));
	return len + 1;
}

static void apply_config_value(struct strbuf *tok, struct strbuf *val, struct trailer_info *info)
{
	if (info->value) {
		strbuf_reset(tok);
		strbuf_addstr(tok, info->value);
	}
}

static void apply_config_list_values(struct strbuf *tok, struct strbuf *val)
{
	int j, tok_alnum_len = alnum_len(tok->buf, tok->len);

	for (j = 0; j < trailer_list.nr; j++) {
		struct string_list_item *item = trailer_list.items + j;
		struct trailer_info *info = item->util;
		if (!strncasecmp(tok->buf, item->string, tok_alnum_len) ||
		    !strncasecmp(tok->buf, info->value, tok_alnum_len)) {
			apply_config_value(tok, val, info);
			break;
		}
	}
}

static struct strbuf **read_input_file(const char *infile)
{
	struct strbuf sb = STRBUF_INIT;

	if (strbuf_read_file(&sb, infile, 0) < 0)
		die_errno(_("could not read input file '%s'"), infile);

	return strbuf_split(&sb, '\n');
}

/*
 * Return the the (0 based) index of the first trailer line
 * or the line count if there are no trailers.
 */
static int find_trailer_start(struct strbuf **lines)
{
	int count, start, empty = 1;

	/* Get the line count */
	for (count = 0; lines[count]; count++);

	/*
	 * Get the start of the trailers by looking starting from the end
	 * for a line with only spaces before lines with one ':'.
	 */
	for (start = count - 1; start >= 0; start--) {
		if (strbuf_isspace(lines[start])) {
			if (empty)
				continue;
			return start + 1;
		}
		if (strchr(lines[start]->buf, ':')) {
			if (empty)
				empty = 0;
			continue;
		}
		return count;
	}

	return empty ? count : start + 1;
}

static void print_tok_val(const char *tok_buf, size_t tok_len,
			  const char *val_buf, size_t val_len)
{
	char c = tok_buf[tok_len - 1];
	if (isalnum(c))
		printf("%s: %s\n", tok_buf, val_buf);
	else if (isspace(c) || c == '#')
		printf("%s%s\n", tok_buf, val_buf);
	else
		printf("%s %s\n", tok_buf, val_buf);
}

static void process_input_file(const char *infile,
			       struct string_list *tok_list,
			       struct string_list *val_list)
{
	struct strbuf **lines = read_input_file(infile);
	int start = find_trailer_start(lines);
	int i;

	/* Print non trailer lines as is */
	for (i = 0; lines[i] && i < start; i++) {
		printf("%s", lines[i]->buf);
	}

	/* Parse trailer lines */
	for (i = start; lines[i]; i++) {
		parse_trailer_into_string_lists(tok_list, val_list, lines[i]->buf);
	}
}

int cmd_interpret_trailers(int argc, const char **argv, const char *prefix)
{
	const char *infile = NULL;
	int trim_empty = 0;
	int i;
	struct string_list tok_list = STRING_LIST_INIT_NODUP;
	struct string_list val_list = STRING_LIST_INIT_NODUP;
	char *applied_arg;

	struct option options[] = {
		OPT_BOOL(0, "trim-empty", &trim_empty, N_("trim empty trailers")),
		OPT_FILENAME(0, "infile", &infile, N_("use message from file")),
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options,
			     git_interpret_trailers_usage, 0);

	git_config(git_trailer_config, NULL);

	/* Print the non trailer part of infile */
	if (infile) {
		process_input_file(infile, &tok_list, &val_list);
		applied_arg = xcalloc(tok_list.nr, 1);
	}

	for (i = 0; i < argc; i++) {
		struct strbuf tok = STRBUF_INIT;
		struct strbuf val = STRBUF_INIT;
		int j, seen = 0;

		parse_trailer(&tok, &val, argv[i]);

		apply_config_list_values(&tok, &val);

		/* Apply the trailer arguments to the trailers in infile */
		for (j = 0; j < tok_list.nr; j++) {
			struct string_list_item *tok_item = tok_list.items + j;
			struct string_list_item *val_item = val_list.items + j;
			int tok_alnum_len = alnum_len(tok.buf, tok.len);
			if (!strncasecmp(tok.buf, tok_item->string, tok_alnum_len)) {
				tok_item->string = xstrdup(tok.buf);
				val_item->string = xstrdup(val.buf);
				seen = 1;
				applied_arg[j] = 1;
				break;
			}
		}

		/* Print the trailer arguments that are not in infile */
		if (!seen && (!trim_empty || val.len > 0))
			print_tok_val(tok.buf, tok.len, val.buf, val.len);

		strbuf_release(&tok);
		strbuf_release(&val);
	}

	/* Print the trailer part of infile */
	for (i = 0; i < tok_list.nr; i++) {
		struct strbuf tok = STRBUF_INIT;
		struct strbuf val = STRBUF_INIT;
		strbuf_addstr(&tok, tok_list.items[i].string);
		strbuf_addstr(&val, val_list.items[i].string);

		if (!applied_arg[i])
			apply_config_list_values(&tok, &val);

		if (!trim_empty || val.len > 0)
			print_tok_val(tok.buf, tok.len, val.buf, val.len);
	}

	return 0;
}
