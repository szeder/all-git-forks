#include "cache.h"
#include "oid-array.h"

void oid_array_init(struct oid_array *array)
{
	array->oid = NULL;
	array->nr = 0;
	array->alloc = 0;
}

void oid_array_clear(struct oid_array *array)
{
	free(array->oid);
	oid_array_init(array);
}

void oid_array_append(struct oid_array *array, const struct object_id *oid)
{
	ALLOC_GROW(array->oid, array->nr + 1, array->alloc);
	oidcpy(&array->oid[array->nr++], oid);
}

size_t oid_array_find(const struct oid_array *array, const struct object_id *oid)
{
	size_t i;
	for (i = 0; i < array->nr; i++)
		if (!oidcmp(&array->oid[i], oid))
			return i;
	return (size_t)-1;
}
