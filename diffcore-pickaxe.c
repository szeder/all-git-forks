/*
 * Copyright (C) 2005 Junio C Hamano
 * Copyright (C) 2010 Google Inc.
 */
#include "cache.h"
#include "diff.h"
#include "diffcore.h"
#include "xdiff-interface.h"
#include "kwset.h"

typedef int (*pickaxe_fn)(struct diff_filepair *p, struct diff_options *o, regex_t *regexp, kwset_t kws);

static void pickaxe(struct diff_queue_struct *q, struct diff_options *o,
		    regex_t *regexp, kwset_t kws, pickaxe_fn fn)
{
	int i;
	struct diff_queue_struct outq;

	DIFF_QUEUE_CLEAR(&outq);

	if (o->pickaxe_opts & DIFF_PICKAXE_ALL) {
		/* Showing the whole changeset if needle exists */
		for (i = 0; i < q->nr; i++) {
			struct diff_filepair *p = q->queue[i];
			if (fn(p, o, regexp, kws))
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
			if (fn(p, o, regexp, kws))
				diff_q(&outq, p);
			else
				diff_free_filepair(p);
		}
	}

	free(q->queue);
	*q = outq;
}

static int pickaxe_setup(struct diff_filepair *p,
			 struct diff_options *o,
			 mmfile_t *mf_one,
			 mmfile_t *mf_two,
			 unsigned *what_to_free)
{
	struct userdiff_driver *textconv_one = NULL;
	struct userdiff_driver *textconv_two = NULL;

	if (!o->pickaxe[0])
		return 0;

	if (DIFF_OPT_TST(o, ALLOW_TEXTCONV)) {
		textconv_one = get_textconv(p->one);
		textconv_two = get_textconv(p->two);
	}

	/*
	 * If we have an unmodified pair, we know that there is no
	 * interesting difference and we don't even have to load the
	 * blobs, unless textconv is in play, _and_ we are using two
	 * different textconv filters (e.g., because a pair is an
	 * exact rename with different textconv attributes for each
	 * side, which might generate different content).
	 */
	if (textconv_one == textconv_two && diff_unmodified_pair(p))
		return 0;

	mf_one->size = fill_textconv(textconv_one, p->one, &mf_one->ptr);
	mf_two->size = fill_textconv(textconv_two, p->two, &mf_two->ptr);

	*what_to_free = (textconv_one ? 1 : 0) | (textconv_two ? 2 : 0);
	return 1;
}

static void pickaxe_finish_filepair(struct diff_filepair *p,
				    mmfile_t *mf_one,
				    mmfile_t *mf_two,
				    unsigned what_to_free)
{
	if (what_to_free & 1)
		free(mf_one->ptr);
	if (what_to_free & 2)
		free(mf_two->ptr);
	diff_free_filespec_data(p->one);
	diff_free_filespec_data(p->two);
}

struct diffgrep_cb {
	regex_t *regexp;
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

static int diff_grep(struct diff_filepair *p, struct diff_options *o,
		     regex_t *regexp, kwset_t kws)
{
	regmatch_t regmatch;
	mmfile_t mf1, mf2;
	unsigned what_to_free;
	int hit;

	if (!pickaxe_setup(p, o, &mf1, &mf2, &what_to_free))
		return 0;

	if (!DIFF_FILE_VALID(p->one)) {
		if (!DIFF_FILE_VALID(p->two))
			hit = 0; /* ignore unmerged */
		else
			/* created "two" -- does it have what we are looking for? */
			hit = !regexec(regexp, mf2.ptr, 1, &regmatch, 0);
	} else if (!DIFF_FILE_VALID(p->two)) {
		/* removed "one" -- did it have what we are looking for? */
		hit = !regexec(regexp, mf1.ptr, 1, &regmatch, 0);
	} else {
		/*
		 * We have both sides; need to run textual diff and see if
		 * the pattern appears on added/deleted lines.
		 */
		struct diffgrep_cb ecbdata;
		xpparam_t xpp;
		xdemitconf_t xecfg;

		memset(&xpp, 0, sizeof(xpp));
		memset(&xecfg, 0, sizeof(xecfg));
		ecbdata.regexp = regexp;
		ecbdata.hit = 0;
		xecfg.ctxlen = o->context;
		xecfg.interhunkctxlen = o->interhunkcontext;
		xdi_diff_outf(&mf1, &mf2, diffgrep_consume, &ecbdata,
			      &xpp, &xecfg);
		hit = ecbdata.hit;
	}

	pickaxe_finish_filepair(p, &mf1, &mf2, what_to_free);
	return hit;
}

static void diffcore_pickaxe_grep(struct diff_options *o)
{
	int err;
	regex_t regex;
	int cflags = REG_EXTENDED | REG_NEWLINE;

	if (DIFF_OPT_TST(o, PICKAXE_IGNORE_CASE))
		cflags |= REG_ICASE;

	err = regcomp(&regex, o->pickaxe, cflags);
	if (err) {
		char errbuf[1024];
		regerror(err, &regex, errbuf, 1024);
		regfree(&regex);
		die("invalid log-grep regex: %s", errbuf);
	}

	pickaxe(&diff_queued_diff, o, &regex, NULL, diff_grep);

	regfree(&regex);
	return;
}

static unsigned int contains(mmfile_t *mf, struct diff_options *o,
			     regex_t *regexp, kwset_t kws)
{
	unsigned int cnt;
	unsigned long sz;
	const char *data;

	sz = mf->size;
	data = mf->ptr;
	cnt = 0;

	if (regexp) {
		regmatch_t regmatch;
		int flags = 0;

		assert(data[sz] == '\0');
		while (*data && !regexec(regexp, data, 1, &regmatch, flags)) {
			flags |= REG_NOTBOL;
			data += regmatch.rm_eo;
			if (*data && regmatch.rm_so == regmatch.rm_eo)
				data++;
			cnt++;
		}

	} else { /* Classic exact string match */
		while (sz) {
			struct kwsmatch kwsm;
			size_t offset = kwsexec(kws, data, sz, &kwsm);
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

static int has_changes(struct diff_filepair *p, struct diff_options *o,
		       regex_t *regexp, kwset_t kws)
{
	mmfile_t mf1, mf2;
	unsigned what_to_free;
	int ret;

	if (!pickaxe_setup(p, o, &mf1, &mf2, &what_to_free))
		return 0;

	if (!DIFF_FILE_VALID(p->one)) {
		if (!DIFF_FILE_VALID(p->two))
			ret = 0; /* ignore unmerged */
		else
			/* created */
			ret = contains(&mf2, o, regexp, kws) != 0;
	}
	else if (!DIFF_FILE_VALID(p->two)) /* removed */
		ret = contains(&mf1, o, regexp, kws) != 0;
	else
		ret = contains(&mf1, o, regexp, kws) !=
		      contains(&mf2, o, regexp, kws);

	pickaxe_finish_filepair(p, &mf1, &mf2, what_to_free);
	return ret;
}

static void diffcore_pickaxe_count(struct diff_options *o)
{
	const char *needle = o->pickaxe;
	int opts = o->pickaxe_opts;
	unsigned long len = strlen(needle);
	regex_t regex, *regexp = NULL;
	kwset_t kws = NULL;

	if (opts & DIFF_PICKAXE_REGEX) {
		int err;
		err = regcomp(&regex, needle, REG_EXTENDED | REG_NEWLINE);
		if (err) {
			/* The POSIX.2 people are surely sick */
			char errbuf[1024];
			regerror(err, &regex, errbuf, 1024);
			regfree(&regex);
			die("invalid pickaxe regex: %s", errbuf);
		}
		regexp = &regex;
	} else {
		kws = kwsalloc(DIFF_OPT_TST(o, PICKAXE_IGNORE_CASE)
			       ? tolower_trans_tbl : NULL);
		kwsincr(kws, needle, len);
		kwsprep(kws);
	}

	pickaxe(&diff_queued_diff, o, regexp, kws, has_changes);

	if (opts & DIFF_PICKAXE_REGEX)
		regfree(&regex);
	else
		kwsfree(kws);
	return;
}

void diffcore_pickaxe(struct diff_options *o)
{
	/* Might want to warn when both S and G are on; I don't care... */
	if (o->pickaxe_opts & DIFF_PICKAXE_KIND_G)
		diffcore_pickaxe_grep(o);
	else
		diffcore_pickaxe_count(o);
}
