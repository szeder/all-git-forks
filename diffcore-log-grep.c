/*
 * Copyright (C) 2010 Junio C Hamano
 */
#include "cache.h"
#include "diff.h"
#include "diffcore.h"
#include "xdiff-interface.h"

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

static void fill_one(struct diff_filespec *one,
		     mmfile_t *mf, struct userdiff_driver **textconv)
{
	if (DIFF_FILE_VALID(one)) {
		*textconv = get_textconv(one);
		mf->size = fill_textconv(*textconv, one, &mf->ptr);
	} else {
		memset(mf, 0, sizeof(*mf));
	}
}

static int diff_grep(struct diff_filepair *p, regex_t *regexp)
{
	regmatch_t regmatch;
	struct userdiff_driver *textconv_one = NULL;
	struct userdiff_driver *textconv_two = NULL;
	mmfile_t mf1, mf2;
	int hit;

	if (diff_unmodified_pair(p))
		return 0;

	fill_one(p->one, &mf1, &textconv_one);
	fill_one(p->two, &mf2, &textconv_two);

	if (!mf1.ptr) {
		if (!mf2.ptr)
			return 0; /* ignore unmerged */
		/* created "two" -- does it have what we are looking for? */
		hit = !regexec(regexp, p->two->data, 1, &regmatch, 0);
	} else if (!mf2.ptr) {
		/* removed "one" -- did it have what we are looking for? */
		hit = !regexec(regexp, p->one->data, 1, &regmatch, 0);
	} else {
		/*
		 * We have both sides; need to run textual diff and see if a
		 * line that match the pattern appears in +/- line.
		 */
		struct diffgrep_cb ecbdata;
		xpparam_t xpp;
		xdemitconf_t xecfg;

		memset(&xpp, 0, sizeof(xpp));
		memset(&xecfg, 0, sizeof(xecfg));
		ecbdata.regexp = regexp;
		ecbdata.hit = 0;
		xdi_diff_outf(&mf1, &mf2, diffgrep_consume, &ecbdata,
			      &xpp, &xecfg);
		hit = ecbdata.hit;
	}
	if (textconv_one)
		free(mf1.ptr);
	if (textconv_two)
		free(mf2.ptr);
	return hit;
}

void diffcore_log_grep(const char *needle, int opts)
{
	struct diff_queue_struct *q = &diff_queued_diff;
	int i, has_changes, err;
	regex_t regex, *regexp = NULL;
	struct diff_queue_struct outq;
	outq.queue = NULL;
	outq.nr = outq.alloc = 0;

	err = regcomp(&regex, needle, REG_EXTENDED | REG_NEWLINE);
	if (err) {
		char errbuf[1024];
		regerror(err, &regex, errbuf, 1024);
		regfree(&regex);
		die("invalid log-grep regex: %s", errbuf);
	}
	regexp = &regex;

	if (opts & DIFF_PICKAXE_ALL) {
		/* Showing the whole changeset if needle exists */
		for (i = has_changes = 0; !has_changes && i < q->nr; i++) {
			struct diff_filepair *p = q->queue[i];
			if (diff_grep(p, regexp))
				has_changes++;
		}
		if (has_changes)
			return; /* not munge the queue */

		/* otherwise we will clear the whole queue
		 * by copying the empty outq at the end of this
		 * function, but first clear the current entries
		 * in the queue.
		 */
		for (i = 0; i < q->nr; i++)
			diff_free_filepair(q->queue[i]);
	} else {
		/* Showing only the filepairs that has the needle */
		for (i = 0; i < q->nr; i++) {
			struct diff_filepair *p = q->queue[i];
			if (diff_grep(p, regexp))
				diff_q(&outq, p);
			else
				diff_free_filepair(p);
		}
	}

	if (opts & DIFF_PICKAXE_REGEX) {
		regfree(&regex);
	}

	free(q->queue);
	*q = outq;
	return;
}
