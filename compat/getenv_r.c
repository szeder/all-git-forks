#include "../git-compat-util.h"

#ifndef NO_PTHREADS
#include <pthread.h>
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#define lock() pthread_mutex_lock(&mutex)
#define unlock() pthread_mutex_unlock(&mutex);
#else
#define lock()
#define unlock()
#endif

int gitgetenv_r(const char *name, char *buf, size_t len)
{
	size_t val_len;
	char *val;

	lock();
	val = getenv(name);
	if (!val) {
		errno = ENOENT;
		return -1;
	}

	val_len = strlen(val) + 1;
	if (val_len > len) {
		errno = ERANGE;
		return -1;
	}

	strcpy(buf, val);
	unlock();

	return 0;
}
