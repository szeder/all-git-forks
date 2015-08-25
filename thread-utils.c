#include "cache.h"
#include "thread-utils.h"
#include "run-command.h"
#include "git-compat-util.h"

#if defined(hpux) || defined(__hpux) || defined(_hpux)
#  include <sys/pstat.h>
#endif

/*
 * By doing this in two steps we can at least get
 * the function to be somewhat coherent, even
 * with this disgusting nest of #ifdefs.
 */
#ifndef _SC_NPROCESSORS_ONLN
#  ifdef _SC_NPROC_ONLN
#    define _SC_NPROCESSORS_ONLN _SC_NPROC_ONLN
#  elif defined _SC_CRAY_NCPU
#    define _SC_NPROCESSORS_ONLN _SC_CRAY_NCPU
#  endif
#endif

int online_cpus(void)
{
#ifdef _SC_NPROCESSORS_ONLN
	long ncpus;
#endif

#ifdef GIT_WINDOWS_NATIVE
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	if ((int)info.dwNumberOfProcessors > 0)
		return (int)info.dwNumberOfProcessors;
#elif defined(hpux) || defined(__hpux) || defined(_hpux)
	struct pst_dynamic psd;

	if (!pstat_getdynamic(&psd, sizeof(psd), (size_t)1, 0))
		return (int)psd.psd_proc_cnt;
#elif defined(HAVE_BSD_SYSCTL) && defined(HW_NCPU)
	int mib[2];
	size_t len;
	int cpucount;

	mib[0] = CTL_HW;
#  ifdef HW_AVAILCPU
	mib[1] = HW_AVAILCPU;
	len = sizeof(cpucount);
	if (!sysctl(mib, 2, &cpucount, &len, NULL, 0))
		return cpucount;
#  endif /* HW_AVAILCPU */
	mib[1] = HW_NCPU;
	len = sizeof(cpucount);
	if (!sysctl(mib, 2, &cpucount, &len, NULL, 0))
		return cpucount;
#endif /* defined(HAVE_BSD_SYSCTL) && defined(HW_NCPU) */

#ifdef _SC_NPROCESSORS_ONLN
	if ((ncpus = (long)sysconf(_SC_NPROCESSORS_ONLN)) > 0)
		return (int)ncpus;
#endif

	return 1;
}

int init_recursive_mutex(pthread_mutex_t *m)
{
	pthread_mutexattr_t a;
	int ret;

	ret = pthread_mutexattr_init(&a);
	if (!ret) {
		ret = pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
		if (!ret)
			ret = pthread_mutex_init(m, &a);
		pthread_mutexattr_destroy(&a);
	}
	return ret;
}

#ifndef NO_PTHREADS
struct job_list {
	int (*fct)(struct task_queue *tq, void *task);
	void *task;
	struct job_list *next;
};

static pthread_t main_thread;
static int main_thread_set;
static pthread_key_t async_key;
static pthread_key_t async_die_counter;

static NORETURN void die_async(const char *err, va_list params)
{
	vreportf("fatal: ", err, params);

	if (!pthread_equal(main_thread, pthread_self()))
		pthread_exit((void *)128);

	exit(128);
}

static int async_die_is_recursing(void)
{
	void *ret = pthread_getspecific(async_die_counter);
	pthread_setspecific(async_die_counter, (void *)1);
	return ret != NULL;
}

/* FIXME: deduplicate this code with run-command.c */
static void setup_main_thread(void)
{
	if (!main_thread_set) {
		main_thread_set = 1;
		main_thread = pthread_self();
		pthread_key_create(&async_key, NULL);
		pthread_key_create(&async_die_counter, NULL);
		set_die_routine(die_async);
		set_die_is_recursing_routine(async_die_is_recursing);
	}
}

struct task_queue {
	/*
	 * To avoid deadlocks always acquire the semaphores with lowest priority
	 * first, priorites are in descending order as listed.
	 *
	 * The `mutex` is a general purpose lock for modifying data in the async
	 * queue, such as adding a new task or adding a return value from
	 * an already run task.
	 *
	 * `workingcount` and `freecount` are opposing semaphores, the sum of
	 * their values should equal `max_threads` at any time while the `mutex`
	 * is available.
	 */
	sem_t mutex;
	sem_t workingcount;
	sem_t freecount;

	pthread_t *threads;
	unsigned max_threads;

	struct job_list *first;
	struct job_list *last;

	void (*finish_function)(struct task_queue *tq);
	int early_return;
};

static void next_task(struct task_queue *tq,
		      int (**fct)(struct task_queue *tq, void *task),
		      void **task,
		      int *early_return)
{
	struct job_list *job = NULL;

	sem_wait(&tq->workingcount);
	sem_wait(&tq->mutex);

	if (*early_return) {
		tq->early_return |= *early_return;
		*fct = NULL;
		*task = NULL;
	} else {
		if (!tq->first)
			die("BUG: internal error with dequeuing jobs for threads");

		job = tq->first;
		*fct = job->fct;
		*task = job->task;

		tq->first = job->next;
		if (!tq->first)
			tq->last = NULL;
	}

	sem_post(&tq->freecount);
	sem_post(&tq->mutex);

	free(job);
}

static void *dispatcher(void *args)
{
	void *task;
	int (*fct)(struct task_queue *tq, void *task);
	int early_return = 0;
	struct task_queue *tq = args;

	next_task(tq, &fct, &task, &early_return);
	while (fct || early_return != 0) {
		early_return = fct(tq, task);
		next_task(tq, &fct, &task, &early_return);
	}

	if (tq->finish_function)
		tq->finish_function(tq);

	pthread_exit(0);
}

struct task_queue *create_task_queue(unsigned max_threads)
{
	struct task_queue *tq = xmalloc(sizeof(*tq));

	int i, ret;
	if (!max_threads)
		tq->max_threads = online_cpus();
	else
		tq->max_threads = max_threads;

	sem_init(&tq->mutex, 0, 1);
	sem_init(&tq->workingcount, 0, 0);
	sem_init(&tq->freecount, 0, tq->max_threads);
	tq->threads = xmalloc(tq->max_threads * sizeof(pthread_t));

	tq->first = NULL;
	tq->last = NULL;

	setup_main_thread();

	for (i = 0; i < tq->max_threads; i++) {
		ret = pthread_create(&tq->threads[i], 0, &dispatcher, tq);
		if (ret)
			die("unable to create thread: %s", strerror(ret));
	}

	tq->early_return = 0;

	return tq;
}

void add_task(struct task_queue *tq,
	      int (*fct)(struct task_queue *tq, void *task),
	      void *task)
{
	struct job_list *job_list;

	job_list = xmalloc(sizeof(*job_list));
	job_list->task = task;
	job_list->fct = fct;
	job_list->next = NULL;

	sem_wait(&tq->freecount);
	sem_wait(&tq->mutex);

	if (!tq->last) {
		tq->last = job_list;
		tq->first = tq->last;
	} else {
		tq->last->next = job_list;
		tq->last = tq->last->next;
	}

	sem_post(&tq->workingcount);
	sem_post(&tq->mutex);
}

int finish_task_queue(struct task_queue *tq, void (*fct)(struct task_queue *tq))
{
	int ret;
	int i;

	tq->finish_function = fct;

	for (i = 0; i < tq->max_threads; i++)
		add_task(tq, NULL, NULL);

	for (i = 0; i < tq->max_threads; i++)
		pthread_join(tq->threads[i], 0);

	sem_destroy(&tq->mutex);
	sem_destroy(&tq->workingcount);
	sem_destroy(&tq->freecount);

	if (tq->first)
		die("BUG: internal error with queuing jobs for threads");

	free(tq->threads);
	ret = tq->early_return;

	free(tq);
	return ret;
}
#else /* NO_PTHREADS */

struct task_queue {
	int early_return;
};

struct task_queue *create_task_queue(unsigned max_threads)
{
	struct task_queue *tq = xmalloc(sizeof(*tq));

	tq->early_return = 0;
}

void add_task(struct task_queue *tq,
	      int (*fct)(struct task_queue *tq, void *task),
	      void *task)
{
	if (tq->early_return)
		return;

	tq->early_return |= fct(tq, task);
}

int finish_task_queue(struct task_queue *tq, void (*fct)(struct task_queue *tq))
{
	int ret = tq->early_return;
	free(tq);
	return ret;
}
#endif
