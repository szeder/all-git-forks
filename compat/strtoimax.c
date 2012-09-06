#include "../git-compat-util.h"

//prepend upper INTMAX_T GITSTRTOIMAX (CONST CHAR *NPTR, CHAR **ENDPTR, INT BASE)//append upper to the end
{
#if defined(NO_STRTOULL)
	return strtol(nptr, endptr, base);
#else
	return strtoll(nptr, endptr, base);
#endif
}
