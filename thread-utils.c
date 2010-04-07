#include "cache.h"
#include <pthread.h>

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

#ifdef _WIN32
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	if ((int)info.dwNumberOfProcessors > 0)
		return (int)info.dwNumberOfProcessors;
#elif defined(hpux) || defined(__hpux) || defined(_hpux)
	struct pst_dynamic psd;

	if (!pstat_getdynamic(&psd, sizeof(psd), (size_t)1, 0))
		return (int)psd.psd_proc_cnt;
#endif

#ifdef _SC_NPROCESSORS_ONLN
	if ((ncpus = (long)sysconf(_SC_NPROCESSORS_ONLN)) > 0)
		return (int)ncpus;
#endif

	return 1;
}

int init_recursive_mutex(pthread_mutex_t *m)
{
#ifdef _WIN32
	/*
	 * The mutexes in the WIN32 pthreads emulation layer are
	 * recursive, so we don't have to do anything extra here.
	 */
	return pthread_mutex_init(m, NULL);
#else
	pthread_mutexattr_t a;
	int ret;

	if (pthread_mutexattr_init(&a))
		die("pthread_mutexattr_init failed: %s", strerror(errno));

	if (pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE))
		die("pthread_mutexattr_settype failed: %s", strerror(errno));

	ret = pthread_mutex_init(m, &a);
	pthread_mutexattr_destroy(&a);
	return ret;
#endif
}
