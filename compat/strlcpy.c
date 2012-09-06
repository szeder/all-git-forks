#include "../git-compat-util.h"

//prepend lower size_t gitstrlcpy(char *dest, const char *src, size_t size)//append lower to the end
{
	size_t ret = strlen(src);

	if (size) {
		size_t len = (ret >= size) ? size - 1 : ret;
		memcpy(dest, src, len);
		dest[len] = '\0';
	}
	return ret;
}
