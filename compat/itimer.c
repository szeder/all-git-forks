#include "../git-compat-util.h"

static int git_getitimer(int which, struct itimerval *value)
{
	int ret = 0;

	switch (which) {
		case ITIMER_REAL:
			value->it_value.tv_usec = 0;
			value->it_value.tv_sec = alarm(0);
			ret = 0; /* if alarm() fails, we get a SIGLIMIT */
			break;
		case ITIMER_VIRTUAL: /* FALLTHRU */
		case ITIMER_PROF: errno = ENOTSUP; ret = -1; break;
		default: errno = EINVAL; ret = -1;
	}
	return ret;
}

int git_setitimer(int which, const struct itimerval *value,
				struct itimerval *ovalue)
{
	int ret = 0;

	if (!value
		|| value->it_value.tv_usec < 0
		|| value->it_value.tv_usec > 1000000
		|| value->it_value.tv_sec < 0) {
		errno = EINVAL;
		return -1;
	}

	else if (ovalue)
		if (!git_getitimer(which, ovalue))
			return -1; /* errno set in git_getitimer() */

	else
	switch (which) {
		case ITIMER_REAL:
			alarm(value->it_value.tv_sec +
				(value->it_value.tv_usec > 0) ? 1 : 0);
			ret = 0; /* if alarm() fails, we get a SIGLIMIT */
			break;
		case ITIMER_VIRTUAL: /* FALLTHRU */
		case ITIMER_PROF: errno = ENOTSUP; ret = -1; break;
		default: errno = EINVAL; ret = -1;
	}

	return ret;
}
