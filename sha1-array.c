#include "cache.h"
#include "sha1-array.h"
#include "sha1-lookup.h"

void sha1_array_append(struct sha1_array *array, const unsigned char *sha1)
{
	ALLOC_GROW(array->sha1, array->nr + 1, array->alloc);
	hashcpy(array->sha1[array->nr++], sha1);
	array->sorted = 0;
}

static int void_hashcmp(const void *a, const void *b)
{
	return hashcmp(a, b);
}

static void sha1_array_sort(struct sha1_array *array)
{
	qsort(array->sha1, array->nr, sizeof(*array->sha1), void_hashcmp);
	array->sorted = 1;
}

static const unsigned char *sha1_access(size_t index, void *table)
{
	unsigned char (*array)[20] = table;
	return array[index];
}

int sha1_array_lookup(struct sha1_array *array, const unsigned char *sha1)
{
	if (!array->sorted)
		sha1_array_sort(array);
	return sha1_pos(sha1, array->sha1, array->nr, sha1_access);
}

int sha1_array_remove(struct sha1_array *array, const unsigned char *sha1)
{
	int pos;

	if (!array->sorted)
		sha1_array_sort(array);
	pos = sha1_pos(sha1, array->sha1, array->nr, sha1_access);
	if (pos < 0)
		return pos;

	array->nr--;

	for (; pos < array->nr; ++pos) {
		hashcpy(array->sha1[pos], array->sha1[pos + 1]);
	}

	return 0;
}

void sha1_array_clear(struct sha1_array *array)
{
	free(array->sha1);
	array->sha1 = NULL;
	array->nr = 0;
	array->alloc = 0;
	array->sorted = 0;
}

void sha1_array_for_each_unique(struct sha1_array *array,
				for_each_sha1_fn fn,
				void *data)
{
	int i;

	if (!array->sorted)
		sha1_array_sort(array);

	for (i = 0; i < array->nr; i++) {
		if (i > 0 && !hashcmp(array->sha1[i], array->sha1[i-1]))
			continue;
		fn(array->sha1[i], data);
	}
}

void sha1_array_for_each_nonunique(struct sha1_array *array,
				   for_each_sha1_fn fn,
				   void *data)
{
	int i;
	for (i = 0; i < array->nr; i++) {
		fn(array->sha1[i], data);
	}
}

static void increment_int(const unsigned char *sha1, void *data)
{
	int *count = data;
	(*count)++;
}

int sha1_array_count_unique(struct sha1_array *array)
{
	int count = 0;
	if (!array->sorted)
		sha1_array_sort(array);

	sha1_array_for_each_unique(array, increment_int, &count);
	return count;
}
