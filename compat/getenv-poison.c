#include "../git-compat-util.h"
#undef getenv

char *gitgetenv(const char *name)
{
	static char buf[1000000]; /* 1MB should be plenty */
	char *value = getenv(name);

	if (!value)
		return NULL;

	strncpy(buf, value, sizeof(buf));
	return buf;
}
