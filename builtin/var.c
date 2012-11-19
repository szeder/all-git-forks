/*
 * GIT - The information manager from hell
 *
 * Copyright (C) Eric Biederman, 2005
 */
#include "builtin.h"

static const char var_usage[] = "git var (-l | <variable>...)";

static const char *editor(int flag)
{
	const char *pgm = git_editor();

	if (!pgm && flag & IDENT_STRICT)
		die("Terminal is dumb, but EDITOR unset");

	return pgm;
}

static const char *pager(int flag)
{
	const char *pgm = git_pager(1);

	if (!pgm)
		pgm = "cat";
	return pgm;
}

static const char *explicit_ident(const char * (*get_ident)(int),
				  int (*check_ident)(void))
{
	/*
	 * Prime the "explicit" flag by getting the identity.
	 * We do not want to be strict here, because we would not want
	 * to die on bogus implicit values, but instead just report that they
	 * are not explicit.
	 */
	get_ident(0);
	return check_ident() ? "1" : "0";
}

static const char *committer_explicit(int flag)
{
	return explicit_ident(git_committer_info,
			      committer_ident_sufficiently_given);
}

static const char *author_explicit(int flag)
{
	return explicit_ident(git_author_info,
			      author_ident_sufficiently_given);
}

struct git_var {
	const char *name;
	const char *(*read)(int);
};
static struct git_var git_vars[] = {
	{ "GIT_COMMITTER_IDENT", git_committer_info },
	{ "GIT_COMMITTER_EXPLICIT", committer_explicit },
	{ "GIT_AUTHOR_IDENT",   git_author_info },
	{ "GIT_AUTHOR_EXPLICIT", author_explicit },
	{ "GIT_EDITOR", editor },
	{ "GIT_PAGER", pager },
	{ "", NULL },
};

static void list_vars(void)
{
	struct git_var *ptr;
	const char *val;

	for (ptr = git_vars; ptr->read; ptr++)
		if ((val = ptr->read(0)))
			printf("%s=%s\n", ptr->name, val);
}

static const char *read_var(const char *var)
{
	struct git_var *ptr;
	const char *val;
	val = NULL;
	for (ptr = git_vars; ptr->read; ptr++) {
		if (strcmp(var, ptr->name) == 0) {
			val = ptr->read(IDENT_STRICT);
			break;
		}
	}
	return val;
}

static int show_config(const char *var, const char *value, void *cb)
{
	if (value)
		printf("%s=%s\n", var, value);
	else
		printf("%s\n", var);
	return git_default_config(var, value, cb);
}

int cmd_var(int argc, const char **argv, const char *prefix)
{
	if (argc < 2)
		usage(var_usage);

	if (strcmp(argv[1], "-l") == 0) {
		if (argc > 2)
			usage(var_usage);
		git_config(show_config, NULL);
		list_vars();
		return 0;
	}
	git_config(git_default_config, NULL);

	while (*++argv) {
		const char *val = read_var(*argv);
		if (!val)
			usage(var_usage);
		printf("%s\n", val);
	}

	return 0;
}
