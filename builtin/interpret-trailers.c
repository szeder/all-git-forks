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
#include "trailer.h"

static const char * const git_interpret_trailers_usage[] = {
	N_("git interpret-trailers [--trim-empty] [--infile=<file>] [(<token>[(=|:)<value>])...]"),
	NULL
};

int cmd_interpret_trailers(int argc, const char **argv, const char *prefix)
{
	const char *infile = NULL;
	int trim_empty = 0;

	struct option options[] = {
		OPT_BOOL(0, "trim-empty", &trim_empty, N_("trim empty trailers")),
		OPT_FILENAME(0, "infile", &infile, N_("use message from file")),
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options,
			     git_interpret_trailers_usage, 0);

	process_trailers(infile, trim_empty, argc, argv);

	return 0;
}
