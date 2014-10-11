/*
 * Implementation of isvn parallel fetching.
 *
 * Copyright (c) 2014 Conrad Meyer <cse.cem@gmail.com>
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <stdbool.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

#include <svn_client.h>
#include <svn_cmdline.h>
#include <svn_delta.h>
#include <svn_hash.h>
#include <svn_path.h>
#include <svn_props.h>
#include <svn_pools.h>
#include <svn_ra.h>
#include <svn_repos.h>
#include <svn_string.h>

#define NO_THE_INDEX_COMPATIBILITY_MACROS

#include "builtin.h"
#include "dir.h"
#include "parse-options.h"
#include "remote.h"
#include "thread-utils.h"
#include "transport.h"
#include "help.h"

#include "isvn/isvn-internal.h"

/* Callback hell! */
static svn_delta_editor_t *g_svn_dedit_obj;	/* (u) */

void assert_status_noerr(apr_status_t status, const char *fmt, ...)
{
	char buf[1024];
	va_list ap;

	if (status == 0)
		return;

	fprintf(stderr, "APR failure: ");

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	apr_strerror(status, buf, sizeof(buf) - 1);
	fprintf(stderr, ": %s(%ju)", buf, (uintmax_t)status);
	exit(EX_SOFTWARE);
}

void assert_noerr(svn_error_t *err, const char *fmt, ...)
{
	char buf[1024];
	va_list ap;

	if (err == NULL)
		return;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	svn_handle_error2(err, stderr, true, buf);
	exit(EX_SOFTWARE);
}

static void end_svn_ctx(struct isvn_client_ctx *ctx)
{
	svn_pool_destroy(ctx->svn_pool);
	free(ctx);
}

struct isvn_client_ctx *get_svn_ctx(void)
{
	struct isvn_client_ctx *ctx;
	svn_config_t *cfg_servers;
	svn_error_t *err;

	ctx = malloc(sizeof(*ctx));
	if (ctx == NULL)
		die("malloc");

	ctx->svn_pool = apr_allocator_owner_get(svn_pool_create_allocator(true));
	if (ctx->svn_pool == NULL)
		die("(?)apr_allocator_owner_get");

	assert_noerr(svn_ra_initialize(ctx->svn_pool),
		"svn_ra_initialize");
	assert_noerr(svn_config_ensure(NULL, ctx->svn_pool),
		"svn_config_ensure");
	assert_noerr(
		svn_client_create_context2(&ctx->svn_client, NULL,
			ctx->svn_pool),
		"svn_client_create_context2");
	assert_noerr(
		svn_config_get_config(&ctx->svn_client->config, NULL,
			ctx->svn_pool),
		"svn_config_get_config");

	cfg_servers = svn_hash_gets(ctx->svn_client->config,
		SVN_CONFIG_CATEGORY_SERVERS);
	svn_config_set_bool(cfg_servers, SVN_CONFIG_SECTION_GLOBAL,
		SVN_CONFIG_OPTION_HTTP_BULK_UPDATES, true);
	svn_config_set_int64(cfg_servers, SVN_CONFIG_SECTION_GLOBAL,
		SVN_CONFIG_OPTION_HTTP_MAX_CONNECTIONS, 8);

	assert_noerr(
		svn_cmdline_create_auth_baton(&ctx->svn_client->auth_baton,
			true/*non-interactive*/, option_user, option_password,
			NULL/*config_dir*/, false, false,
			svn_hash_gets(ctx->svn_client->config,
				SVN_CONFIG_CATEGORY_CONFIG),
			NULL/*cancel_func*/, NULL/*cancel_func arg*/,
			ctx->svn_pool),
		"svn_cmdline_create_auth_baton");

	err = svn_client_open_ra_session2(&ctx->svn_session,
		g_svn_url, NULL, ctx->svn_client, ctx->svn_pool, ctx->svn_pool);
	if (err) {
		assert_noerr(err, "XXX %s", __func__);
		end_svn_ctx(ctx);
		return NULL;
	}

	return ctx;
}

static int edit_cmp(const struct br_edit *e1, const struct br_edit *e2,
	const void *dummy __unused)
{
	return strcmp(e1->e_path, e2->e_path);
}

void branch_edit_free(struct br_edit *edit)
{
	free(edit->e_path);
	free(edit->e_copyfrom);
	free(edit->e_diff);
	free(edit);
}

void branch_rev_free(struct branch_rev *br)
{
	struct hashmap_iter iter;
	struct br_edit *edit;

	hashmap_iter_init(&br->rv_edits, &iter);
	while ((edit = hashmap_iter_next(&iter))) {
		TAILQ_REMOVE(&br->rv_editorder, edit, e_list);
		branch_edit_free(edit);
	}
	hashmap_free(&br->rv_edits, false);

	/* INVARIANTS */
	if (!TAILQ_EMPTY(&br->rv_editorder))
		die("%s(r%u): not empty: kind:%d path:%s", __func__,
			br->rv_rev, TAILQ_FIRST(&br->rv_editorder)->e_kind,
			TAILQ_FIRST(&br->rv_editorder)->e_path);

	free(br->rv_author);
	free(br->rv_logmsg);
	free(br);
}

static svn_error_t *_isvn_fetch_revend(svn_revnum_t revision, void *cbdata,
	const svn_delta_editor_t *editor, void *edit_data,
	apr_hash_t *rev_props, apr_pool_t *pool)
{
	struct branch_rev *br, *nbr;
	struct hashmap *br_revs;
	struct svn_branch *sb;

	if (option_verbosity > 1)
		fprintf(stderr, "Fetched: r%ju\n", (uintmax_t)revision);

	br_revs = cbdata;
	br = edit_data;

	if (!br->rv_only_empty_dirs) {
		if (br->rv_branch == NULL)
			die("invariants: non-empty changeset has no branch");

		for (; br; br = nbr) {
			sb = svn_branch_get(br_revs, br->rv_branch);
			svn_branch_append(sb, br);

			nbr = br->rv_affil;
			br->rv_affil = NULL;
		}
	} else
		branch_rev_free(br);

	return NULL;
}

struct branch_rev *new_branch_rev(svn_revnum_t rev)
{
	struct branch_rev *br;

	br = xcalloc(1, sizeof(*br));
	br->rv_rev = rev;
	hashmap_init(&br->rv_edits, (hashmap_cmp_fn)edit_cmp, 0);
	TAILQ_INIT(&br->rv_editorder);
	br->rv_only_empty_dirs = true;

	return br;
}

static void branch_get_props(struct branch_rev *br, apr_hash_t *props,
	apr_pool_t *pool)
{
	struct apr_hash_index_t *hi;
	svn_string_t *valstr;
	const void *key;
	apr_ssize_t klen;
	void *val;

#define KEYEQ(str) (klen == strlen((str)) && \
	strncmp(key, (str), strlen((str))) == 0)

	for (hi = apr_hash_first(pool, props); hi; hi = apr_hash_next(hi)) {
		apr_hash_this(hi, &key, &klen, &val);
		valstr = val;

		if (KEYEQ("svn:log"))
			br->rv_logmsg = xstrndup(valstr->data, valstr->len);
		else if (KEYEQ("svn:author"))
			br->rv_author = xstrndup(valstr->data, valstr->len);
		else if (KEYEQ("svn:date")) {
			/* NUL-terminate */
			char *datez = xstrndup(valstr->data, valstr->len);

			if (parse_date_basic(datez, &br->rv_timestamp, NULL) &&
			    option_verbosity >= 0)
				fprintf(stderr, "W: invalid timestamp: %s\n",
				    datez);

			free(datez);
		} else if (KEYEQ("google:author")) {
			/* Ignore, duplicate of svn:author. */
#if 0
			printf("XXX %s(google:author): %.*s\n", __func__,
				(int)valstr->len, valstr->data);
#endif
		} else
			printf("XXX %s: Skipping rev attr '%.*s'\n", __func__,
				(int)klen, (char *)key);
	}
#undef KEYEQ
}

static svn_error_t *_isvn_fetch_revstart(svn_revnum_t revision, void *cbdata,
	const svn_delta_editor_t **editor, void **edit_data,
	apr_hash_t *rev_props, apr_pool_t *pool)
{
	struct branch_rev *br;

	br = new_branch_rev(revision);
	branch_get_props(br, rev_props, pool);

	*editor = g_svn_dedit_obj;
	*edit_data = br;

	printf("XXX %s: r%ju\n", __func__, (uintmax_t)revision);

	return NULL;
}

void *isvn_fetch_worker(void *dummy_i)
{
	struct isvn_client_ctx *client;
	svn_error_t *err;

	struct svn_branch *branch;
	struct hashmap_iter branch_iter;

	struct hashmap branch_revs;
	unsigned i, revlo, revhi;
	bool done;

	done = false;
	i = (uintptr_t)dummy_i;

	client = get_svn_ctx();

	for (;;) {
		/* grab a chunk to work on */
		isvn_fetcher_getrange(&revlo, &revhi, &done);
		if (done)
			break;
		printf("XXX: %s(%u) chose r%u:%u\n", __func__, i, revlo, revhi);

		svn_branch_hash_init(&branch_revs);

		/* Download ranges */
		err = svn_ra_replay_range(client->svn_session, revlo, revhi, 0,
			true /* deltas */, _isvn_fetch_revstart,
			_isvn_fetch_revend, (void *)&branch_revs,
			client->svn_pool);

		/* TODO exponential backoff ??? */
		assert_noerr(err, "svn_ra_replay_range");

		hashmap_iter_init(&branch_revs, &branch_iter);
		while ((branch = hashmap_iter_next(&branch_iter)))
			svn_branch_revs_enqueue_and_free(branch);

		hashmap_free(&branch_revs, false);

		/* Put range into fetchdone hash; if lowest, accumulate and
		 * signal fetchdone_cond */
		isvn_mark_fetchdone(revlo, revhi);
	}

	end_svn_ctx(client);

	return NULL;
}

void isvn_fetch_init(void)
{
	g_svn_dedit_obj = svn_delta_default_editor(g_apr_pool);
	isvn_editor_inialize_dedit_obj(g_svn_dedit_obj);
}
