/*
 * decorate.c - decorate a git object with some arbitrary
 * data.
 */
#include "cache.h"
#include "decorate.h"

static unsigned int hash_obj(const unsigned char *sha1, unsigned int n)
{
	unsigned int hash;

	memcpy(&hash, sha1, sizeof(unsigned int));
	return hash % n;
}

static void *insert_decoration(struct decoration *n, const unsigned char *sha1, void *decoration)
{
	int size = n->size;
	struct object_decoration *hash = n->hash;
	unsigned int j = hash_obj(sha1, size);

	while (!is_null_sha1(hash[j].sha1)) {
		if (hashcmp(hash[j].sha1, sha1) == 0) {
			void *old = hash[j].decoration;
			hash[j].decoration = decoration;
			return old;
		}
		if (++j >= size)
			j = 0;
	}
	hashcpy(hash[j].sha1, sha1);
	hash[j].decoration = decoration;
	n->nr++;
	return NULL;
}

static void grow_decoration(struct decoration *n)
{
	int i;
	int old_size = n->size;
	struct object_decoration *old_hash = n->hash;

	n->size = (old_size + 1000) * 3 / 2;
	n->hash = xcalloc(n->size, sizeof(struct object_decoration));
	n->nr = 0;

	for (i = 0; i < old_size; i++) {
		const unsigned char *sha1 = old_hash[i].sha1;
		void *decoration = old_hash[i].decoration;

		if (is_null_sha1(sha1))
			continue;
		insert_decoration(n, sha1, decoration);
	}
	free(old_hash);
}

/* Add a decoration pointer, return any old one */
void *add_decoration(struct decoration *n, const unsigned char *sha1,
		void *decoration)
{
	int nr = n->nr + 1;

	if (nr > n->size * 2 / 3)
		grow_decoration(n);
	return insert_decoration(n, sha1, decoration);
}

/* Lookup a decoration pointer */
void *lookup_decoration(struct decoration *n, const unsigned char *sha1)
{
	unsigned int j;

	/* nothing to lookup */
	if (!n->size)
		return NULL;
	j = hash_obj(sha1, n->size);
	for (;;) {
		struct object_decoration *ref = n->hash + j;
		if (hashcmp(ref->sha1, sha1) == 0)
			return ref->decoration;
		if (is_null_sha1(ref->sha1))
			return NULL;
		if (++j == n->size)
			j = 0;
	}
}
