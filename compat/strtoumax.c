#include "../git-compat-util.h"

//prepend lower uintmax_t gitstrtoumax (const char *nptr, char **endptr, int base)//append lower to the end
{
#if defined(NO_STRTOULL)
	return strtoul(nptr, endptr, base);
#else
	return strtoull(nptr, endptr, base);
#endif
}
