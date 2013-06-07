#include "cache.h"
#include "commit.h"
#include "commit-queue.h"

void clear_commit_queue(struct commit_queue *queue)
{
	free(queue->array);
	queue->nr = 0;
	queue->alloc = 0;
	queue->array = NULL;
}

void commit_queue_put(struct commit_queue *queue, struct commit *commit)
{
	commit_compare_fn compare = queue->compare;
	int ix, parent;

	/* Append at the end */
	ALLOC_GROW(queue->array, queue->nr + 1, queue->alloc);
	queue->array[queue->nr++] = commit;
	if (!compare)
		return; /* LIFO */

	/* Bubble up the new one */
	for (ix = queue->nr - 1; ix; ix = parent) {
		parent = (ix - 1) / 2;
		if (compare(queue->array[parent], queue->array[ix],
			    queue->cb_data) < 0)
			break;

		commit = queue->array[parent];
		queue->array[parent] = queue->array[ix];
		queue->array[ix] = commit;
	}
}

struct commit *commit_queue_get(struct commit_queue *queue)
{
	struct commit *result, *swap;
	int ix, child;
	commit_compare_fn compare = queue->compare;

	if (!queue->nr)
		return NULL;
	if (!compare)
		return queue->array[--queue->nr]; /* LIFO */

	result = queue->array[0];
	if (!--queue->nr)
		return result;

	queue->array[0] = queue->array[queue->nr];

	/* Push down the one at the root */
	for (ix = 0; ix * 2 + 1 < queue->nr; ix = child) {
		child = ix * 2 + 1; /* left */
		if ((child + 1 < queue->nr) &&
		    (compare(queue->array[child], queue->array[child + 1],
			     queue->cb_data) >= 0))
			child++; /* use right child */

		if (compare(queue->array[ix], queue->array[child],
			    queue->cb_data) < 0)
			break;

		swap = queue->array[child];
		queue->array[child] = queue->array[ix];
		queue->array[ix] = swap;
	}
	return result;
}
