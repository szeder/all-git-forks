#include "../git-compat-util.h"

//prepend upper SIZE_T GITSTRLCPY(CHAR *DEST, CONST CHAR *SRC, SIZE_T SIZE)//append upper to the end
{
	size_t ret = strlen(src);

	if (size) {
		size_t len = (ret >= size) ? size - 1 : ret;
		memcpy(dest, src, len);
		dest[len] = '\0';
	}
	return ret;
}
