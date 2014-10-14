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

#include <stdbool.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

#include <pthread.h>

/* Needed for isvn-internal.h */
#include <svn_client.h>
#include <svn_pools.h>

#include "isvn/isvn-git2.h"
#include "isvn/isvn-internal.h"

/* In-core revmap */
static pthread_rwlock_t g_revmap_lk;
static struct hashmap g_revmap;

struct revmap_entry {
	struct hashmap_entry	 re_entry;
	unsigned		 re_rev;
	unsigned		 re_nbr;

	struct rebr {
		git_oid		 rb_sha1;
		const char	*rb_branch;
	}			 re_branches[1];
};

static int
revmap_ent_cmp(const struct revmap_entry *r1, const struct revmap_entry *r2,
    const void *dummy __unused)
{
	return (int)r1->re_rev - (int)r2->re_rev;
}

void
isvn_revmap_init(void)
{
	hashmap_init(&g_revmap, (hashmap_cmp_fn)revmap_ent_cmp, 0);
	rw_init(&g_revmap_lk);
}

void
isvn_revmap_insert(unsigned revnum, const char *branch, const git_oid *sha1)
{
	struct revmap_entry *re, *exist;
	unsigned nbr, i;

	branch = strintern(branch);

	re = xcalloc(1, sizeof(*re));
	re->re_rev = revnum;
	hashmap_entry_init(&re->re_entry, memhash(&revnum, sizeof(revnum)));

	re->re_nbr = 1;
	git_oid_cpy(&re->re_branches[0].rb_sha1, sha1);
	re->re_branches[0].rb_branch = branch;

	rw_wlock(&g_revmap_lk);

	exist = hashmap_get(&g_revmap, re, NULL);
	if (exist) {
		free(re);

		nbr = exist->re_nbr;

		/* Don't double-add */
		for (i = 0; i < nbr; i++)
			/* Ptr-equality fine for interned strings. */
			if (exist->re_branches[i].rb_branch == branch)
				goto out;

		/* Slow-case, but multiple branches per revmap is SO uncommon.
		 * Allocate under lock to add our branch to the existing array. */

		/* In case realloc moves the object: */
		hashmap_remove(&g_revmap, exist, NULL);

		exist = xrealloc(exist, sizeof(*re) +
		    nbr * sizeof(exist->re_branches[0]));

		git_oid_cpy(&exist->re_branches[nbr].rb_sha1, sha1);
		exist->re_branches[nbr].rb_branch = branch;

		exist->re_nbr = nbr + 1;

		hashmap_add(&g_revmap, exist);
	} else
		hashmap_add(&g_revmap, re);

out:
	rw_unlock(&g_revmap_lk);
}

int
isvn_revmap_lookup_branchlatest(const char *branch, unsigned rev,
    git_oid *sha1_out)
{
	struct revmap_entry *re, lookup;
	unsigned scan, i;

	scan = 0;
	re = NULL;
	branch = strintern(branch);

	rw_rlock(&g_revmap_lk);
	for (; rev > 0; rev--) {
		lookup.re_rev = rev;
		hashmap_entry_init(&lookup.re_entry, memhash(&rev, sizeof(rev)));

		re = hashmap_get(&g_revmap, &lookup, NULL);
		if (re) {
			for (i = 0; i < re->re_nbr; i++) {
				if (re->re_branches[i].rb_branch == branch) {
					git_oid_cpy(sha1_out,
					    &re->re_branches[i].rb_sha1);
					goto out;
				}
			}
		}

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

out:
	rw_unlock(&g_revmap_lk);

	if (re)
		return 0;
	return -1;
}

/* For debugging */
void
isvn_dump_revmap(void)
{
	struct revmap_entry *entry;
	struct hashmap_iter iter;
	char tmpbuf[50];
	unsigned i;

	printf("ISVN Revmap dump:\n");

	rw_rlock(&g_revmap_lk);

	hashmap_iter_init(&g_revmap, &iter);
	while ((entry = hashmap_iter_next(&iter))) {
		for (i = 0; i < entry->re_nbr; i++) {
			git_oid_fmt(tmpbuf, &entry->re_branches[i].rb_sha1);
			printf("%s\tr%u\t%s\n", tmpbuf, entry->re_rev,
			    entry->re_branches[i].rb_branch);
		}
	}

	rw_unlock(&g_revmap_lk);
}
