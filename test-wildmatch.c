/*
 * Test suite for the wildmatch code.
 *
 * Copyright (C) 2003-2009 Wayne Davison
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, visit the http://fsf.org website.
 */

/*#define COMPARE_WITH_FNMATCH*/

#include "cache.h"
#include "parse-options.h"
#include "wildmatch.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif
#ifdef NO_STRLCPY
#include "compat/strlcpy.c"
#define strlcpy gitstrlcpy
#endif

#ifdef COMPARE_WITH_FNMATCH
#include <fnmatch.h>

int fnmatch_errors = 0;
#endif

int wildmatch_errors = 0;
char number_separator = ',';

typedef char bool;

int explode_mod = 0;
int empties_mod = 0;
int empty_at_start = 0;
int empty_at_end = 0;
char *empties;

static struct option options[] = {
  OPT_STRING('e', "empties", &empties, "", ""),
  OPT_INTEGER('x', "explode", &explode_mod, ""),
  OPT_END(),
};

/* match just at the start of string (anchored tests) */
static void
run_test(int line, bool matches,
#ifdef COMPARE_WITH_FNMATCH
	 bool same_as_fnmatch,
#endif
	 const char *text, const char *pattern)
{
    bool matched;
#ifdef COMPARE_WITH_FNMATCH
    bool fn_matched;
    int flags = strstr(pattern, "**")? 0 : FNM_PATHNAME;
#endif

    if (explode_mod) {
	char buf[MAXPATHLEN*2], *texts[MAXPATHLEN];
	int pos = 0, cnt = 0, ndx = 0, len = strlen(text);

	if (empty_at_start)
	    texts[ndx++] = "";
	/* An empty string must turn into at least one empty array item. */
	while (1) {
	    texts[ndx] = buf + ndx * (explode_mod + 1);
	    strlcpy(texts[ndx++], text + pos, explode_mod + 1);
	    if (pos + explode_mod >= len)
		break;
	    pos += explode_mod;
	    if (!(++cnt % empties_mod))
		texts[ndx++] = "";
	}
	if (empty_at_end)
	    texts[ndx++] = "";
	texts[ndx] = NULL;
	matched = wildmatch_array(pattern, (const char**)texts, 0);
    } else
	matched = wildmatch(pattern, text);
#ifdef COMPARE_WITH_FNMATCH
    fn_matched = !fnmatch(pattern, text, flags);
#endif
    if (matched != matches) {
	printf("wildmatch failure on line %d:\n  %s\n  %s\n  expected %s match\n",
	       line, text, pattern, matches? "a" : "NO");
	wildmatch_errors++;
    }
#ifdef COMPARE_WITH_FNMATCH
    if (fn_matched != (matches ^ !same_as_fnmatch)) {
	printf("fnmatch disagreement on line %d:\n  %s\n  %s\n  expected %s match\n",
	       line, text, pattern, matches ^ !same_as_fnmatch? "a" : "NO");
	fnmatch_errors++;
    }
#endif
}

int
main(int argc, char **argv)
{
    char buf[2048], *s, *string[2], *end[2];
    FILE *fp;
    int line, i, flag[2];
    const char *help[] = { NULL };

    argc = parse_options(argc, (const char **)argv, "", options, help, 0);
    if (argc != 1)
	    die("redundant options");
    if (empties) {
	const char *arg = empties;
	empties_mod = atoi(arg);
	if (strchr(empties, 's'))
	    empty_at_start = 1;
	if (strchr(arg, 'e'))
	    empty_at_end = 1;
	if (!explode_mod)
	    explode_mod = 1024;
    }

    if (explode_mod && !empties_mod)
	empties_mod = 1024;

    if ((fp = fopen(*argv, "r")) == NULL) {
	fprintf(stderr, "Unable to open %s\n", *argv);
	exit(1);
    }

    line = 0;
    while (fgets(buf, sizeof buf, fp)) {
	line++;
	if (*buf == '#' || *buf == '\n')
	    continue;
	for (s = buf, i = 0; i <= 1; i++) {
	    if (*s == '1')
		flag[i] = 1;
	    else if (*s == '0')
		flag[i] = 0;
	    else
		flag[i] = -1;
	    if (*++s != ' ' && *s != '\t')
		flag[i] = -1;
	    if (flag[i] < 0) {
		fprintf(stderr, "Invalid flag syntax on line %d of %s:\n%s",
			line, *argv, buf);
		exit(1);
	    }
	    while (*++s == ' ' || *s == '\t') {}
	}
	for (i = 0; i <= 1; i++) {
	    if (*s == '\'' || *s == '"' || *s == '`') {
		char quote = *s++;
		string[i] = s;
		while (*s && *s != quote) s++;
		if (!*s) {
		    fprintf(stderr, "Unmatched quote on line %d of %s:\n%s",
			    line, *argv, buf);
		    exit(1);
		}
		end[i] = s;
	    }
	    else {
		if (!*s || *s == '\n') {
		    fprintf(stderr, "Not enough strings on line %d of %s:\n%s",
			    line, *argv, buf);
		    exit(1);
		}
		string[i] = s;
		while (*++s && *s != ' ' && *s != '\t' && *s != '\n') {}
		end[i] = s;
	    }
	    while (*++s == ' ' || *s == '\t') {}
	}
	*end[0] = *end[1] = '\0';
	run_test(line, flag[0],
#ifdef COMPARE_WITH_FNMATCH
		 flag[1],
#endif
		 string[0], string[1]);
    }

    if (!wildmatch_errors)
	fputs("No", stdout);
    else
	printf("%d", wildmatch_errors);
    printf(" wildmatch error%s found.\n", wildmatch_errors == 1? "" : "s");

#ifdef COMPARE_WITH_FNMATCH
    if (!fnmatch_errors)
	fputs("No", stdout);
    else
	printf("%d", fnmatch_errors);
    printf(" fnmatch error%s found.\n", fnmatch_errors == 1? "" : "s");

#endif

    return 0;
}
