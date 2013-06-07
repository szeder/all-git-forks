#ifndef PRIO_QUEUE_H
#define PRIO_QUEUE_H

/*
 * Priority queue implementation, primarily for keeping track of
 * commits in the 'date-order' so that we process them from new to old
 * as they are discovered, but can be used to hold any pointer to
 * structure.  The caller is responsible for supplying a function to
 * compare two "things".
 *
 * Alternatively, this datastructure can also be used as a LIFO stack
 * by specifying NULL as the comparison funciton.
 */

/*
 * Compare two "things", one and two; the third parameter is cb_data
 * in the prio_queue structure.  The result is returned as a sign of the
 * return value, being the same as sign of "one" - "two" (i.e. negative
 * if one sorts earlier than two.
 */
typedef int (*prio_queue_compare_fn)(const void *one, const void *two, void *cb_data);

struct prio_queue {
	prio_queue_compare_fn compare;
	void *cb_data;
	int alloc, nr;
	void **array;
};

/*
 * Add the "thing" to the queue.
 */
extern void prio_queue_put(struct prio_queue *, void *thing);

/*
 * Extract the "thing" that compares the smallest out of the queue,
 * or NULL.  If compare function is NULL, the queue acts as a LIFO
 * stack.
 */
extern void *prio_queue_get(struct prio_queue *);

extern void clear_prio_queue(struct prio_queue *);

#endif /* PRIO_QUEUE_H */
