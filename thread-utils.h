#ifndef THREAD_COMPAT_H
#define THREAD_COMPAT_H

#ifndef NO_PTHREADS
#include <pthread.h>

extern int online_cpus(void);
extern int init_recursive_mutex(pthread_mutex_t*);

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>

#else

#define online_cpus() 1

#endif

/*
 * Creates a struct `task_queue`, which holds a list of tasks. Up to
 * `max_threads` threads are active to process the enqueued tasks
 * processing the tasks in a first in first out order.
 *
 * If `max_threads` is zero the number of cores available will be used.
 *
 * Currently this only works in environments with pthreads, in other
 * environments, the task will be processed sequentially in `add_task`.
 */
struct task_queue *create_task_queue(unsigned max_threads);

/*
 * The function and data are put into the task queue.
 *
 * The function `fct` must not be NULL, as that's used internally
 * in `finish_task_queue` to signal shutdown. If the return code
 * of `fct` is unequal to 0, the tasks will stop eventually,
 * the current parallel tasks will be flushed out.
 */
void add_task(struct task_queue *tq,
	      int (*fct)(struct task_queue *tq, void *task),
	      void *task);

/*
 * Waits for all tasks to be done and frees the object. The return code
 * is zero if all enqueued tasks were processed.
 *
 * The function `fct` is called once in each thread after the last task
 * for that thread was processed. If no thread local cleanup needs to be
 * performed, pass NULL.
 */
int finish_task_queue(struct task_queue *tq, void (*fct)(struct task_queue *tq));

#endif /* THREAD_COMPAT_H */
