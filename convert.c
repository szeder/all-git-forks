#include "cache.h"
/*
 * convert.c - convert a file when checking it out and checking it in.
 *
 * This should use the pathname to decide on whether it wants to do some
 * more interesting conversions (automatic gzip/unzip, general format
 * conversions etc etc), but by default it just does automatic CRLF<->LF
 * translation when the "auto_crlf" option is set.
 */

/*
 * Auto and forced CRLF conversion
 */

/*
 * Copy *in to *out while stripping '\r' from it.  The caller should
 * make sure that out is large enough.
 */
static int convi_crlf_real(char *in, char *out, unsigned long isize)
{
	do {
		unsigned char c = *in++;
		if (c != '\r')
			*out++ = c;
	} while (--isize);
	return 1;
}

/*
 * Copy *in to *out while munging '\r\n' to '\n'.  The caller should
 * make sure that out is large enough.
 */
static int convo_crlf_real(char *in, char *out, unsigned long isize)
{
	unsigned char last = 0;
	do {
		unsigned char c = *in++;
		if (c == '\n' && last != '\r')
			*out++ = '\r';
		*out++ = c;
		last = c;
	} while (--isize);
	return 1;
}

/*
 * Forced CRLF conversion.
 */
static int convi_crlf(const char *path, char **bufp, unsigned long *sizep)
{
	unsigned long size, cr, i, nsize;
	char *buf, *nbuf;

	size = *sizep;
	buf = *bufp;
	for (i = cr = 0; i < size; i++) {
		unsigned char c = buf[i];
		if (c == '\r')
			cr++;
	}
	nsize = size - cr;
	*sizep = nsize;
	nbuf = xmalloc(nsize);
	*bufp = nbuf;
	return convi_crlf_real(buf, nbuf, size);
}

static int convo_crlf(const char *path, char **bufp, unsigned long *sizep)
{
	unsigned long size, cr, i, nsize;
	unsigned char last;
	char *buf, *nbuf;

	size = *sizep;
	buf = *bufp;
	last = 0;

	for (i = cr = 0; i < size; i++) {
		unsigned char c = buf[i];
		if (c == '\n' && last != '\r')
			cr++;
		last = c;
	}
	nsize = size + cr;
	nbuf = xmalloc(nsize);
	*sizep = nsize;
	*bufp = nbuf;
	return convo_crlf_real(buf, nbuf, size);
}

/*
 * Auto CRLF conversion.
 */
struct text_stat {
	/* CR, LF and CRLF counts */
	unsigned cr, lf, crlf;

	/* These are just approximations! */
	unsigned printable, nonprintable;
};

static void gather_stats(const char *buf, unsigned long size, struct text_stat *stats)
{
	unsigned long i;

	memset(stats, 0, sizeof(*stats));

	for (i = 0; i < size; i++) {
		unsigned char c = buf[i];
		if (c == '\r') {
			stats->cr++;
			if (i+1 < size && buf[i+1] == '\n')
				stats->crlf++;
			continue;
		}
		if (c == '\n') {
			stats->lf++;
			continue;
		}
		if (c == 127)
			/* DEL */
			stats->nonprintable++;
		else if (c < 32) {
			switch (c) {
				/* BS, HT, ESC and FF */
			case '\b': case '\t': case '\033': case '\014':
				stats->printable++;
				break;
			default:
				stats->nonprintable++;
			}
		}
		else
			stats->printable++;
	}
}

static int is_binary(unsigned long size, struct text_stat *stats)
{

	if ((stats->printable >> 7) < stats->nonprintable)
		return 1;
	/*
	 * Other heuristics? Average line length might be relevant,
	 * as might LF vs CR vs CRLF counts..
	 *
	 * NOTE! It might be normal to have a low ratio of CRLF to LF
	 * (somebody starts with a LF-only file and edits it with an editor
	 * that adds CRLF only to lines that are added..). But do  we
	 * want to support CR-only? Probably not.
	 */
	return 0;
}

static int convi_autocrlf(const char *path, char **bufp, unsigned long *sizep)
{
	char *buffer, *nbuf;
	unsigned long size, nsize;
	struct text_stat stats;

	if (!auto_crlf)
		return 0;

	size = *sizep;
	if (!size)
		return 0;
	buffer = *bufp;

	gather_stats(buffer, size, &stats);

	/* No CR? Nothing to convert, regardless. */
	if (!stats.cr)
		return 0;

	/*
	 * We're currently not going to even try to convert stuff
	 * that has bare CR characters. Does anybody do that crazy
	 * stuff?
	 */
	if (stats.cr != stats.crlf)
		return 0;

	/*
	 * And add some heuristics for binary vs text, of course...
	 */
	if (is_binary(size, &stats))
		return 0;

	/*
	 * Ok, allocate a new buffer, fill it in, and return true
	 * to let the caller know that we switched buffers on it.
	 */
	nsize = size - stats.crlf;
	nbuf = xmalloc(nsize);
	*sizep = nsize;
	*bufp = nbuf;
	return convi_crlf_real(buffer, nbuf, size);
}

static int convo_autocrlf(const char *path, char **bufp, unsigned long *sizep)
{
	char *buffer, *nbuf;
	unsigned long size, nsize;
	struct text_stat stats;

	if (auto_crlf <= 0)
		return 0;

	size = *sizep;
	if (!size)
		return 0;
	buffer = *bufp;

	gather_stats(buffer, size, &stats);

	/* No LF? Nothing to convert, regardless. */
	if (!stats.lf)
		return 0;

	/* Was it already in CRLF format? */
	if (stats.lf == stats.crlf)
		return 0;

	/* If we have any bare CR characters, we're not going to touch it */
	if (stats.cr != stats.crlf)
		return 0;

	if (is_binary(size, &stats))
		return 0;

	/*
	 * Ok, allocate a new buffer, fill it in, and return true
	 * to let the caller know that we switched buffers on it.
	 */
	nsize = size + stats.lf - stats.crlf;
	nbuf = xmalloc(nsize);
	*sizep = nsize;
	*bufp = nbuf;
	return convo_crlf_real(buffer, nbuf, size);
}

/*
 * Other conversions come here
 */

/******************************************************************/

typedef int (*convi_fn)(const char *, char **, unsigned long *);
typedef int (*convo_fn)(const char *, char **, unsigned long *);

static convi_fn pathattr_find_conv_i(const char *path)
{
	/* A silly example */
	if (!fnmatch("*.dos-txt", path, 0))
		return convi_crlf;
	return NULL;
}

static convo_fn pathattr_find_conv_o(const char *path)
{
	/* A silly example */
	if (!fnmatch("*.dos-txt", path, 0))
		return convo_crlf;
	return NULL;
}

/*
 * Main entry points
 */
int convert_to_git(const char *path, char **bufp, unsigned long *sizep)
{
	if (path) {
		convi_fn conv_i = pathattr_find_conv_i(path);
		if (conv_i)
			return conv_i(path, bufp, sizep);
	}
	/* Fallback */
	return convi_autocrlf(path, bufp, sizep);
}

int convert_to_working_tree(const char *path, char **bufp,
			    unsigned long *sizep)
{
	if (path) {
		convi_fn conv_o = pathattr_find_conv_o(path);
		if (conv_o)
			return conv_o(path, bufp, sizep);
	}
	/* Fallback */
	return convo_autocrlf(path, bufp, sizep);
}
