#ifndef COMMIT_QUEUE_H
#define COMMIT_QUEUE_H

/*
 * Compare two commits; the third parameter is cb_data in the
 * commit_queue structure.
 */
typedef int (*commit_compare_fn)(struct commit *, struct commit *, void *);

struct commit_queue {
	commit_compare_fn compare;
	void *cb_data;
	int alloc, nr;
	struct commit **array;
};

/*
 * Add the commit to the queue
 */
extern void commit_queue_put(struct commit_queue *, struct commit *);

/*
 * Extract the commit that compares the smallest out of the queue,
 * or NULL.  If compare function is NULL, the queue acts as a LIFO
 * stack.
 */
extern struct commit *commit_queue_get(struct commit_queue *);

extern void clear_commit_queue(struct commit_queue *);

/* Reverse the LIFO elements */
extern void commit_queue_reverse(struct commit_queue *);

#endif /* COMMIT_QUEUE_H */
