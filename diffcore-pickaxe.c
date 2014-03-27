/*
 * Copyright (C) 2005 Junio C Hamano
 * Copyright (C) 2010 Google Inc.
 */
#include "cache.h"
#include "diff.h"
#include "diffcore.h"
#include "xdiff-interface.h"
#include "kwset.h"
#include "userdiff.h"

struct fn_options {
	regex_t *regex;
	kwset_t kws;
	const struct userdiff_funcname *funcname_pattern;
};

typedef int (*pickaxe_fn)(mmfile_t *one, mmfile_t *two,
			  struct diff_options *o,
			  struct fn_options *fno);

static void compile_regex(regex_t *r, const char *s, int cflags);

static int pickaxe_match(struct diff_filepair *p, struct diff_options *o,
			 pickaxe_fn fn, struct fn_options *fno);

static void pickaxe(struct diff_queue_struct *q, struct diff_options *o,
		    pickaxe_fn fn, struct fn_options *fno)
{
	int i;
	struct diff_queue_struct outq;

	DIFF_QUEUE_CLEAR(&outq);

	if (o->pickaxe_opts & DIFF_PICKAXE_ALL) {
		/* Showing the whole changeset if needle exists */
		for (i = 0; i < q->nr; i++) {
			struct diff_filepair *p = q->queue[i];
			if (pickaxe_match(p, o, fn, fno))
				return; /* do not munge the queue */
		}

		/*
		 * Otherwise we will clear the whole queue by copying
		 * the empty outq at the end of this function, but
		 * first clear the current entries in the queue.
		 */
		for (i = 0; i < q->nr; i++)
			diff_free_filepair(q->queue[i]);
	} else {
		/* Showing only the filepairs that has the needle */
		for (i = 0; i < q->nr; i++) {
			struct diff_filepair *p = q->queue[i];
			if (pickaxe_match(p, o, fn, fno))
				diff_q(&outq, p);
			else
				diff_free_filepair(p);
		}
	}

	free(q->queue);
	*q = outq;
}

struct diffgrep_cb {
	regex_t *regexp;
	int hit;
};

struct funcname_cb {
	struct userdiff_funcname *pattern;
	regex_t *regex;
	int hit;
};

static void diffgrep_consume(void *priv, char *line, unsigned long len)
{
	struct diffgrep_cb *data = priv;
	regmatch_t regmatch;
	int hold;

	if (line[0] != '+' && line[0] != '-')
		return;
	if (data->hit)
		/*
		 * NEEDSWORK: we should have a way to terminate the
		 * caller early.
		 */
		return;
	/* Yuck -- line ought to be "const char *"! */
	hold = line[len];
	line[len] = '\0';
	data->hit = !regexec(data->regexp, line + 1, 1, &regmatch, 0);
	line[len] = hold;
}

static void match_funcname(void *priv, char *line, unsigned long len)
{
	regmatch_t regmatch;
	int hold;
	struct funcname_cb *data = priv;
	hold = line[len];
	line[len] = '\0';

	if (line[0] == '@' && line[1] == '@')
		if (!regexec(data->regex, line, 1, &regmatch, 0))
			data->hit = 1;
	line[len] = hold;
}

static int diff_grep(mmfile_t *one, mmfile_t *two,
		     struct diff_options *o,
		     struct fn_options *fno)
{
	regmatch_t regmatch;
	struct diffgrep_cb ecbdata;
	xpparam_t xpp;
	xdemitconf_t xecfg;

	if (!one)
		return !regexec(fno->regex, two->ptr, 1, &regmatch, 0);
	if (!two)
		return !regexec(fno->regex, one->ptr, 1, &regmatch, 0);

	/*
	 * We have both sides; need to run textual diff and see if
	 * the pattern appears on added/deleted lines.
	 */
	memset(&xpp, 0, sizeof(xpp));
	memset(&xecfg, 0, sizeof(xecfg));
	ecbdata.regexp = fno->regex;
	ecbdata.hit = 0;
	xecfg.ctxlen = o->context;
	xecfg.interhunkctxlen = o->interhunkcontext;
	xdi_diff_outf(one, two, diffgrep_consume, &ecbdata,
		      &xpp, &xecfg);
	return ecbdata.hit;
}

static int diff_funcname_filter(mmfile_t *one, mmfile_t *two,
				struct diff_options *o,
				struct fn_options *fno)
{
	struct funcname_cb ecbdata;
	xpparam_t xpp;
	xdemitconf_t xecfg;

	mmfile_t empty;
	empty.ptr = "";
	empty.size = 0;
	if (!one)
		one = &empty;
	if (!two)
		two = &empty;
	memset(&xpp, 0, sizeof(xpp));
	memset(&xecfg, 0, sizeof(xecfg));
	ecbdata.regex = fno->regex;
	ecbdata.hit = 0;
	xecfg.ctxlen = o->context;

	if (fno->funcname_pattern)
		xdiff_set_find_func(&xecfg, fno->funcname_pattern->pattern,
					    fno->funcname_pattern->cflags);
	xecfg.interhunkctxlen = o->interhunkcontext;
	if (!(one && two))
		xecfg.flags = XDL_EMIT_FUNCCONTEXT;
	xecfg.flags |= XDL_EMIT_FUNCNAMES | XDL_EMIT_MOREFUNCNAMES
		    | XDL_EMIT_MOREHUNKHEADS;
	xdi_diff_outf(one, two, match_funcname, &ecbdata, &xpp, &xecfg);
	return ecbdata.hit;
}

static void diffcore_pickaxe_grep(struct diff_options *o)
{
	regex_t regex;
	struct fn_options fno;
	int cflags = REG_EXTENDED | REG_NEWLINE;

	if (DIFF_OPT_TST(o, PICKAXE_IGNORE_CASE))
		cflags |= REG_ICASE;

	compile_regex(&regex, o->pickaxe, cflags);

	fno.regex = &regex;
	pickaxe(&diff_queued_diff, o, diff_grep, &fno);

	regfree(&regex);
	return;
}

static unsigned int contains(mmfile_t *mf, struct fn_options *fno)
{
	unsigned int cnt;
	unsigned long sz;
	const char *data;

	sz = mf->size;
	data = mf->ptr;
	cnt = 0;

	if (fno->regex) {
		regmatch_t regmatch;
		int flags = 0;

		assert(data[sz] == '\0');
		while (*data && !regexec(fno->regex, data, 1, &regmatch, flags)) {
			flags |= REG_NOTBOL;
			data += regmatch.rm_eo;
			if (*data && regmatch.rm_so == regmatch.rm_eo)
				data++;
			cnt++;
		}

	} else { /* Classic exact string match */
		while (sz) {
			struct kwsmatch kwsm;
			size_t offset = kwsexec(fno->kws, data, sz, &kwsm);
			const char *found;
			if (offset == -1)
				break;
			else
				found = data + offset;
			sz -= found - data + kwsm.size[0];
			data = found + kwsm.size[0];
			cnt++;
		}
	}
	return cnt;
}

static int has_changes(mmfile_t *one, mmfile_t *two,
		       struct diff_options *o,
		       struct fn_options *fno)
{
	unsigned int one_contains = one ? contains(one, fno) : 0;
	unsigned int two_contains = two ? contains(two, fno) : 0;
	return one_contains != two_contains;
}

static void compile_regex(regex_t *r, const char *s, int cflags)
{
	int err;
	err = regcomp(r, s, cflags);
	if (err) {
		char errbuf[1024];
		regerror(err, r, errbuf, 1024);
		regfree(r);
		die("invalid regex: %s", errbuf);
	}
}

static int pickaxe_match(struct diff_filepair *p, struct diff_options *o,
			 pickaxe_fn fn, struct fn_options *fno)
{
	struct userdiff_driver *textconv_one = NULL;
	struct userdiff_driver *textconv_two = NULL;
	mmfile_t mf1, mf2;
	int ret;

	if (o->pickaxe && !o->pickaxe[0])
		return 0;

	/* ignore unmerged */
	if (!DIFF_FILE_VALID(p->one) && !DIFF_FILE_VALID(p->two))
		return 0;

	if (DIFF_OPT_TST(o, ALLOW_TEXTCONV)) {
		textconv_one = get_textconv(p->one);
		textconv_two = get_textconv(p->two);
	}

	/*
	 * If we have an unmodified pair, we know that the count will be the
	 * same and don't even have to load the blobs. Unless textconv is in
	 * play, _and_ we are using two different textconv filters (e.g.,
	 * because a pair is an exact rename with different textconv attributes
	 * for each side, which might generate different content).
	 */
	if (textconv_one == textconv_two && diff_unmodified_pair(p))
		return 0;

	const struct userdiff_funcname *funcname_pattern;
	funcname_pattern = diff_funcname_pattern(p->one);
	if (!funcname_pattern)
		funcname_pattern = diff_funcname_pattern(p->two);

	fno->funcname_pattern = funcname_pattern;

	mf1.size = fill_textconv(textconv_one, p->one, &mf1.ptr);
	mf2.size = fill_textconv(textconv_two, p->two, &mf2.ptr);

	ret = fn(DIFF_FILE_VALID(p->one) ? &mf1 : NULL,
		 DIFF_FILE_VALID(p->two) ? &mf2 : NULL,
		 o, fno);

	if (textconv_one)
		free(mf1.ptr);
	if (textconv_two)
		free(mf2.ptr);
	diff_free_filespec_data(p->one);
	diff_free_filespec_data(p->two);

	return ret;
}

static void diffcore_pickaxe_count(struct diff_options *o)
{
	struct fn_options fno;
	const char *needle = o->pickaxe;
	int opts = o->pickaxe_opts;
	unsigned long len = strlen(needle);
	regex_t regex, *regexp = NULL;
	kwset_t kws = NULL;

	if (opts & DIFF_PICKAXE_REGEX) {
		compile_regex(&regex, needle, REG_EXTENDED | REG_NEWLINE);
		regexp = &regex;
	} else {
		kws = kwsalloc(DIFF_OPT_TST(o, PICKAXE_IGNORE_CASE)
			       ? tolower_trans_tbl : NULL);
		kwsincr(kws, needle, len);
		kwsprep(kws);
	}

	fno.regex = regexp;
	fno.kws = kws;
	pickaxe(&diff_queued_diff, o, has_changes, &fno);

	if (opts & DIFF_PICKAXE_REGEX)
		regfree(&regex);
	else
		kwsfree(kws);
	return;
}

static void diffcore_funcname(struct diff_options *o)
{
	struct fn_options fno;
	regex_t regex;

	fno.regex = &regex;
	compile_regex(&regex, o->funcname, REG_EXTENDED | REG_NEWLINE);
	pickaxe(&diff_queued_diff, o, diff_funcname_filter, &fno);
	regfree(&regex);
}

void diffcore_pickaxe(struct diff_options *o)
{
	if (o->funcname)
		diffcore_funcname(o);
	/* Might want to warn when both S and G are on; I don't care... */
	if (o->pickaxe_opts & DIFF_PICKAXE_KIND_G)
		diffcore_pickaxe_grep(o);
	else if (o-> pickaxe_opts & DIFF_PICKAXE_KIND_S)
		diffcore_pickaxe_count(o);
}
