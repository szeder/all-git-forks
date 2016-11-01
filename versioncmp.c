#include "cache.h"
#include "string-list.h"

/*
 * versioncmp(): copied from string/strverscmp.c in glibc commit
 * ee9247c38a8def24a59eb5cfb7196a98bef8cfdc, reformatted to Git coding
 * style. The implementation is under LGPL-2.1 and Git relicenses it
 * to GPLv2.
 */

/*
 * states: S_N: normal, S_I: comparing integral part, S_F: comparing
 * fractionnal parts, S_Z: idem but with leading Zeroes only
 */
#define  S_N    0x0
#define  S_I    0x3
#define  S_F    0x6
#define  S_Z    0x9

/* result_type: CMP: return diff; LEN: compare using len_diff/diff */
#define  CMP    2
#define  LEN    3

static const struct string_list *prereleases;
static int initialized;

/*
 * off is the offset of the first different character in the two strings
 * s1 and s2. If either s1 or s2 contains a prerelease suffix starting
 * at that offset or the character at that offset is part of a
 * prerelease suffix, then that string will be forced to be on top.
 *
 * If both s1 and s2 contain a (different) suffix at that position, the
 * order is determined by config file.
 *
 * Note that we don't have to deal with the situation when both s1 and
 * s2 contain the same suffix because the common part is already
 * consumed by the caller.
 *
 * Return non-zero if *diff contains the return value for versioncmp()
 */
static int swap_prereleases(const char *s1,
			    const char *s2,
			    int off,
			    int *diff)
{
	int i, i1 = -1, i2 = -1;

	for (i = 0; i < prereleases->nr; i++) {
		const char *suffix = prereleases->items[i].string;
		int j, start, suffix_len = strlen(suffix);
		if (suffix_len < off)
			start = off - suffix_len + 1;
		else
			start = 0;
		for (j = start; j <= off; j++) {
			if (i1 == -1 && starts_with(s1 + j, suffix)) {
				i1 = i;
				break;
			}
		}
		for (j = start; j <= off; j++) {
			if (i2 == -1 && starts_with(s2 + j, suffix)) {
				i2 = i;
				break;
			}
		}
	}
	if (i1 == -1 && i2 == -1)
		return 0;
	if (i1 >= 0 && i2 >= 0)
		*diff = i1 - i2;
	else if (i1 >= 0)
		*diff = -1;
	else /* if (i2 >= 0) */
		*diff = 1;
	return 1;
}

/*
 * Compare S1 and S2 as strings holding indices/version numbers,
 * returning less than, equal to or greater than zero if S1 is less
 * than, equal to or greater than S2 (for more info, see the texinfo
 * doc).
 */

int versioncmp(const char *s1, const char *s2)
{
	const unsigned char *p1 = (const unsigned char *) s1;
	const unsigned char *p2 = (const unsigned char *) s2;
	unsigned char c1, c2;
	int state, diff;

	/*
	 * Symbol(s)    0       [1-9]   others
	 * Transition   (10) 0  (01) d  (00) x
	 */
	static const uint8_t next_state[] = {
		/* state    x    d    0  */
		/* S_N */  S_N, S_I, S_Z,
		/* S_I */  S_N, S_I, S_I,
		/* S_F */  S_N, S_F, S_F,
		/* S_Z */  S_N, S_F, S_Z
	};

	static const int8_t result_type[] = {
		/* state   x/x  x/d  x/0  d/x  d/d  d/0  0/x  0/d  0/0  */

		/* S_N */  CMP, CMP, CMP, CMP, LEN, CMP, CMP, CMP, CMP,
		/* S_I */  CMP, -1,  -1,  +1,  LEN, LEN, +1,  LEN, LEN,
		/* S_F */  CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP, CMP,
		/* S_Z */  CMP, +1,  +1,  -1,  CMP, CMP, -1,  CMP, CMP
	};

	if (p1 == p2)
		return 0;

	c1 = *p1++;
	c2 = *p2++;
	/* Hint: '0' is a digit too.  */
	state = S_N + ((c1 == '0') + (isdigit (c1) != 0));

	while ((diff = c1 - c2) == 0) {
		if (c1 == '\0')
			return diff;

		state = next_state[state];
		c1 = *p1++;
		c2 = *p2++;
		state += (c1 == '0') + (isdigit (c1) != 0);
	}

	if (!initialized) {
		initialized = 1;
		prereleases = git_config_get_value_multi("versionsort.prereleasesuffix");
	}
	if (prereleases && swap_prereleases(s1, s2, (const char *) p1 - s1 - 1,
					    &diff))
		return diff;

	state = result_type[state * 3 + (((c2 == '0') + (isdigit (c2) != 0)))];

	switch (state) {
	case CMP:
		return diff;

	case LEN:
		while (isdigit (*p1++))
			if (!isdigit (*p2++))
				return 1;

		return isdigit (*p2) ? -1 : diff;

	default:
		return state;
	}
}
