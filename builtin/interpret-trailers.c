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
	N_("git interpret-trailers [--trim-empty] [<token[=value]>...]"),
	NULL
};

static void parse_arg(struct strbuf *tok, struct strbuf *val, const char *arg)
{
	char *end = strchr(arg, '=');
	if (!end)
		end = strchr(arg, ':');
	if (end) {
		strbuf_add(tok, arg, end - arg);
		strbuf_trim(tok);
		strbuf_addstr(val, end + 1);
		strbuf_trim(val);
	} else {
		strbuf_addstr(tok, arg);
		strbuf_trim(tok);
	}
}

static struct string_list trailer_list;

enum trailer_conf { ADD, UNIQ };

struct trailer_info {
	char *value;
	char *command;
	enum trailer_conf conf;
};

static int git_trailer_config(const char *key, const char *value, void *cb)
{
	if (!prefixcmp(key, "trailer.")) {
		const char *orig_key = key;
		char *name;
		struct string_list_item *item;
		struct trailer_info *info;
		enum { VALUE, CONF, COMMAND } type;

		key += 8;
		if (!suffixcmp(key, ".value")) {
			name = xstrndup(key, strlen(key) - 6);
			type = VALUE;
		} else if (!suffixcmp(key, ".conf")) {
			name = xstrndup(key, strlen(key) - 5);
			type = CONF;
		} else if (!suffixcmp(key, ".command")) {
			name = xstrndup(key, strlen(key) - 8);
			type = COMMAND;
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
		} else if (type == CONF) {
			if (!strcasecmp("add", value)) {
				info->conf = ADD;
			} else if (!strcasecmp("uniq", value)) {
				info->conf = UNIQ;
			} else
				warning(_("unknow value '%s' for key '%s'"), value, orig_key);
		} else {
			if (info->command)
				warning(_("more than one %s"), orig_key);
			info->command = xstrdup(value);
		}
	}
	return 0;
}

static void apply_config(struct strbuf *tok, struct strbuf *val, struct trailer_info *info)
{
	if (info->value) {
		strbuf_reset(tok);
		strbuf_addstr(tok, info->value);
	}
	if (info->command) {
	}
}

int cmd_interpret_trailers(int argc, const char **argv, const char *prefix)
{
	int i, j, trim_empty = 0;
	struct option options[] = {
		OPT_BOOL(0, "trim-empty", &trim_empty, N_("trim empty trailers")),
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options,
			     git_interpret_trailers_usage, 0);

	git_config(git_trailer_config, NULL);

	for (i = 0; i < argc; i++) {
		struct strbuf tok = STRBUF_INIT;
		struct strbuf val = STRBUF_INIT;
		parse_arg(&tok, &val, argv[i]);

		for (j = 0; j < trailer_list.nr; j++) {
			struct string_list_item *item = trailer_list.items + j;
			struct trailer_info *info = item->util;
			int len = isalnum(tok.buf[tok.len - 1]) ? tok.len : tok.len - 1;
			if (!strncasecmp(tok.buf, item->string, len) ||
			    !strncasecmp(tok.buf, info->value, len)) {
				apply_config(&tok, &val, info);
			}
		}

		if (!trim_empty || val.len > 0) {
			char c = tok.buf[tok.len - 1];
			if (isalnum(c))
				printf("%s: %s\n", tok.buf, val.buf);
			else if (isspace(c) || c == '#')
				printf("%s%s\n", tok.buf, val.buf);
			else
				printf("%s %s\n", tok.buf, val.buf);
		}

		strbuf_release(&tok);
		strbuf_release(&val);
	}

	return 0;
}
