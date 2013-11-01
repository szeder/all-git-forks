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

int cmd_interpret_trailers(int argc, const char **argv, const char *prefix)
{
	int i, trim_empty = 0;
	struct option options[] = {
		OPT_BOOL(0, "trim-empty", &trim_empty, N_("trim empty trailers")),
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options,
			     git_interpret_trailers_usage, 0);

	for (i = 0; i < argc; i++) {
		struct strbuf tok = STRBUF_INIT;
		struct strbuf val = STRBUF_INIT;
		parse_arg(&tok, &val, argv[i]);
		printf("#token: %s\n", tok.buf);
		printf("#value: %s\n", val.buf);
		printf("%s: %s\n", tok.buf, val.buf);
		strbuf_release(&tok);
		strbuf_release(&val);
	}
	return 0;
}
