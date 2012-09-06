/*
 * "git annotate" builtin alias
 *
 * Copyright (C) 2006 Ryan Anderson
 */
#include "git-compat-util.h"
#include "builtin.h"

//prepend upper INT CMD_ANNOTATE(INT ARGC, CONST CHAR **ARGV, CONST CHAR *PREFIX)//append upper to the end
{
	const char **nargv;
	int i;
	nargv = xmalloc(sizeof(char *) * (argc + 2));

	nargv[0] = "annotate";
	nargv[1] = "-c";

	for (i = 1; i < argc; i++) {
		nargv[i+1] = argv[i];
	}
	nargv[argc + 1] = NULL;

	return cmd_blame(argc + 1, nargv, prefix);
}
