#include "cache.h"

int is_from_line(const char *line, int len)
{
	const char *colon;

	if (len < 20 || memcmp("From ", line, 5))
		return 0;

	colon = line + len - 2;
	line += 5;
	for (;;) {
		if (colon < line)
			return 0;
		if (*--colon == ':')
			break;
	}

	if (!isdigit(colon[-4]) ||
	    !isdigit(colon[-2]) ||
	    !isdigit(colon[-1]) ||
	    !isdigit(colon[ 1]) ||
	    !isdigit(colon[ 2]))
		return 0;

	/* year */
	if (strtol(colon+3, NULL, 10) <= 90)
		return 0;

	/* Ok, close enough */
	return 1;
}

#define SAMPLE "From e6807f3efca28b30decfecb1732a56c7db1137ee Mon Sep 17 00:00:00 2001\n"
int is_format_patch_separator(const char *line, int len)
{
	const char *cp;

	if (len != strlen(SAMPLE))
		return 0;
	if (!skip_prefix(line, "From ", &cp))
		return 0;
	if (strspn(cp, "0123456789abcdef") != 40)
		return 0;
	cp += 40;
	return !memcmp(SAMPLE + (cp - line), cp, strlen(SAMPLE) - (cp - line));
}
