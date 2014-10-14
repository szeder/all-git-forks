#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "isvn/isvn-hashmap.h"

#define HM_INITIAL_SIZE		16
#define BUCKET(map, nhash)	((nhash) % ((map)->hm_buckets))

/* TODO: OOM */
#define xcalloc calloc

#ifndef __DECONST
#define __DECONST(p, t) (t)(uintptr_t)(p)
#endif

static void _hashmap_resize(struct hashmap *h);

void
hashmap_init(struct hashmap *h, hashmap_cmp_fn f, size_t isize)
{
	(void) isize;

	memset(h, 0, sizeof(*h));

	h->hm_buckets = HM_INITIAL_SIZE;
	h->hm_table = xcalloc(HM_INITIAL_SIZE, sizeof(h->hm_table[0]));
	h->hm_cmpfn = f;
}

void *
hashmap_get(const struct hashmap *h, /* const struct hashmap_entry * */const void *k,
    const void *dummy)
{
	const struct hashmap_entry *ke = k;
	struct hashmap_entry *fe;

	(void)dummy;

	for (fe = SLIST_FIRST(&h->hm_table[ BUCKET(h, ke->he_hash) ]); fe;
	    fe = SLIST_NEXT(fe, he_list)) {
		if (h->hm_cmpfn(fe, k, dummy) == 0)
			break;
	}

	return fe;
}

void *
hashmap_remove(struct hashmap *h, /* const struct hashmap_entry * */const void *k,
    const void *dummy)
{
	const struct hashmap_entry *ke = k;
	struct hashmap_entry *fe, *pe;

	(void)dummy;

	pe = NULL;
	for (fe = SLIST_FIRST(&h->hm_table[ BUCKET(h, ke->he_hash) ]); fe;
	    fe = SLIST_NEXT(fe, he_list)) {
		if (h->hm_cmpfn(fe, k, dummy) == 0)
			break;
		pe = fe;
	}

	if (fe == NULL)
		return NULL;

	if (pe)
		SLIST_REMOVE_AFTER(pe, he_list);
	else
		SLIST_REMOVE_HEAD(
		    &h->hm_table[ BUCKET(h, ke->he_hash) ], he_list);
	h->hm_size--;

	return fe;
}

void
hashmap_add(struct hashmap *h, /*struct hashmap_entry * */ void *v)
{
	struct hashmap_entry *k = v;

	SLIST_INSERT_HEAD(&h->hm_table[ BUCKET(h, k->he_hash) ], k, he_list);
	h->hm_size++;

	if (h->hm_size >= 2 * h->hm_buckets)
		_hashmap_resize(h);
}

void *
hashmap_iter_next(struct hashmap_iter *iter)
{
	const struct hashmap_entry *e;

	while (true) {
		if (iter->hi_next) {
			e = iter->hi_next;
			iter->hi_next = SLIST_NEXT(e, he_list);
			return __DECONST(e, void *);
		}

		if (iter->hi_bucket >= iter->hi_hash->hm_buckets)
			return NULL;

		iter->hi_next =
		    SLIST_FIRST(&iter->hi_hash->hm_table[iter->hi_bucket]);
		iter->hi_bucket++;
	}
}

void
hashmap_free(struct hashmap *h, bool dummy)
{

	free(h->hm_table);
	memset(h, 0, sizeof(*h));
}

static void
_hashmap_resize(struct hashmap *h)
{
	struct hashmap_entry *ent, *save;
	size_t i, oldsz, oldcnt;
	struct _hmehd *old;

	oldsz = h->hm_buckets;
	old = h->hm_table;
	oldcnt = h->hm_size;

	h->hm_buckets *= 2;
	h->hm_table = xcalloc(h->hm_buckets, sizeof(h->hm_table[0]));

	h->hm_size = 0;
	for (i = 0; i < oldsz; i++)
		SLIST_FOREACH_SAFE(ent, &old[i], he_list, save)
			hashmap_add(h, ent);

	/* INVARIANTS */
	if (h->hm_size != oldcnt) {
		/* XXX don't have die() here. Want a different API anyways.
		 * Post-libgit2. */
		fprintf(stderr, "Mismatch after resize: %zu != %zu\n",
		    h->hm_size, oldcnt);
		abort();
	}

	free(old);
}
