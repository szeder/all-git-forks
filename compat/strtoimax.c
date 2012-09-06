#include "../git-compat-util.h"

//prepend lower intmax_t gitstrtoimax (const char *nptr, char **endptr, int base)//append lower to the end
{
#if defined(NO_STRTOULL)
	return strtol(nptr, endptr, base);
#else
	return strtoll(nptr, endptr, base);
#endif
}
