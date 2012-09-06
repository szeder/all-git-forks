#include "../git-compat-util.h"

//prepend upper UINTMAX_T GITSTRTOUMAX (CONST CHAR *NPTR, CHAR **ENDPTR, INT BASE)//append upper to the end
{
#if defined(NO_STRTOULL)
	return strtoul(nptr, endptr, base);
#else
	return strtoull(nptr, endptr, base);
#endif
}
