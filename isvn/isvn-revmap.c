/*
 * Implementation of isvn rev-map (SVN rXXX <-> Git commit sha1 map)
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

/* Needed for isvn-internal.h */
#include <svn_client.h>
#include <svn_pools.h>

#define NO_THE_INDEX_COMPATIBILITY_MACROS

#include "builtin.h"
#include "cache.h"
#include "cache-tree.h"
#include "dir.h"
#include "parse-options.h"
#include "remote.h"
#include "streaming.h"
#include "thread-utils.h"
#include "transport.h"
#include "tree-walk.h"
#include "help.h"

#include "isvn/isvn-internal.h"

/* In-core revmap */
static pthread_rwlock_t g_revmap_lk;
static struct hashmap g_revmap;

struct revmap_entry {
	struct hashmap_entry	 re_entry;
	const char		*re_branch;
	unsigned		 re_rev;
	unsigned char		 re_sha1[20];
};

static int revmap_ent_cmp(const struct revmap_entry *r1,
	const struct revmap_entry *r2, const void *dummy __unused)
{
	return (int)r1->re_rev - (int)r2->re_rev;
}

/* In-core HWM map. (Highest committed rev for branches.) */
static pthread_mutex_t g_hwmmap_mtx;
static struct hashmap g_hwmmap;

struct hwm_entry {
	struct hashmap_entry	 hw_entry;
	const char		*hw_branch;
	unsigned		 hw_rev;
};

static int hwment_cmp(const struct hwm_entry *b1,
	const struct hwm_entry *b2, const void *dummy __unused)
{
	/* Could be ptr comparison if we intern all inputs */
	return strcmp(b1->hw_branch, b2->hw_branch);
}
static void _hwm_bump(const char *branch, unsigned rev);

void isvn_revmap_init(void)
{
	hashmap_init(&g_revmap, (hashmap_cmp_fn)revmap_ent_cmp, 0);
	rw_init(&g_revmap_lk);

	hashmap_init(&g_hwmmap, (hashmap_cmp_fn)hwment_cmp, 0);
	mtx_init(&g_hwmmap_mtx);
}

void isvn_revmap_insert(unsigned revnum, const char *branch,
	unsigned char sha1[20])
{
	struct revmap_entry *re;

	re = xcalloc(1, sizeof(*re));
	re->re_rev = revnum;
	hashcpy(re->re_sha1, sha1);
	hashmap_entry_init(&re->re_entry, memhash(&revnum, sizeof(revnum)));

	branch = strintern(branch);
	re->re_branch = branch;

	rw_wlock(&g_revmap_lk);
	hashmap_add(&g_revmap, re);
	rw_unlock(&g_revmap_lk);

	_hwm_bump(branch, revnum);
}

void isvn_revmap_lookup(unsigned revnum, const char **branch_out,
	unsigned char sha1_out[20])
{
	struct revmap_entry *re, lookup;

	lookup.re_rev = revnum;
	hashmap_entry_init(&lookup.re_entry, memhash(&revnum, sizeof(revnum)));

	re = NULL;
	rw_rlock(&g_revmap_lk);
	re = hashmap_get(&g_revmap, &lookup, NULL);
	rw_unlock(&g_revmap_lk);

	if (re == NULL) {
		if (option_verbosity >= 0)
			printf("W: %s: No such revision r%u?\n", __func__,
				revnum);
		*branch_out = NULL;
		hashclr(sha1_out);
		return;
	}

	*branch_out = re->re_branch;
	hashcpy(sha1_out, re->re_sha1);
}

int isvn_revmap_lookup_branchlatest(const char *branch, unsigned rev,
	unsigned char sha1_out[20])
{
	struct revmap_entry *re, lookup;
	unsigned scan;

	scan = 0;
	re = NULL;
	branch = strintern(branch);

	rw_rlock(&g_revmap_lk);
	for (; rev > 0; rev--) {
		lookup.re_rev = rev;
		hashmap_entry_init(&lookup.re_entry, memhash(&rev, sizeof(rev)));

		re = hashmap_get(&g_revmap, &lookup, NULL);
		if (re && re->re_branch == branch)
			break;

		re = NULL;

		/* Arbitrary -- try to avoid starving writers */
		scan++;
		if (scan >= 100) {
			rw_unlock(&g_revmap_lk);
			usleep(1);
			rw_rlock(&g_revmap_lk);
			scan = 0;
		}
	}
	rw_unlock(&g_revmap_lk);

	if (re) {
		hashcpy(sha1_out, re->re_sha1);
		return 0;
	}
	return -1;
}

/* For debugging */
void isvn_dump_revmap(void)
{
	struct revmap_entry *entry;
	struct hashmap_iter iter;
	char tmpbuf[50];

	printf("ISVN Revmap dump:\n");

	rw_rlock(&g_revmap_lk);

	hashmap_iter_init(&g_revmap, &iter);
	while ((entry = hashmap_iter_next(&iter))) {
		bin_to_hex_buf(entry->re_sha1, tmpbuf, 20);

		printf("%s\tr%u\t%s\n", tmpbuf, entry->re_rev,
			entry->re_branch);
	}

	rw_unlock(&g_revmap_lk);
}

static void _hwm_bump(const char *branch, unsigned rev)
{
	struct hwm_entry *newent, *ex;

	/* INVARIANTS */
	if (branch != strintern(branch))
		die("%s: intern it", __func__);

	newent = xcalloc(1, sizeof(*newent));
	newent->hw_rev = rev;
	newent->hw_branch = strintern(branch);
	hashmap_entry_init(&newent->hw_entry, strhash(branch));

	mtx_lock(&g_hwmmap_mtx);

	ex = hashmap_get(&g_hwmmap, newent, NULL);
	if (!ex) {
		hashmap_add(&g_hwmmap, newent);
		newent = NULL;
	} else if (ex->hw_rev < rev)
		ex->hw_rev = rev;

	mtx_unlock(&g_hwmmap_mtx);

	if (newent)
		free(newent);
}

bool isvn_has_commit(const char *branch, unsigned rev)
{
	struct hwm_entry *re, lookup;

	lookup.hw_branch = strintern(branch);
	hashmap_entry_init(&lookup.hw_entry, strhash(branch));

	mtx_lock(&g_hwmmap_mtx);
	re = hashmap_get(&g_hwmmap, &lookup, NULL);
	mtx_unlock(&g_hwmmap_mtx);

	if (re && re->hw_rev >= rev)
		return true;
	return false;
}

void isvn_assert_commit(const char *branch, unsigned rev)
{
	if (!isvn_has_commit(branch, rev))
		die("%s: %s@%u missing", __func__, branch, rev);
}
