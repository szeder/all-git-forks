#include "cache.h"

#ifdef SHA1_MAX_BLOCK_SIZE
int git_SHA1_Update(SHA_CTX *c, const void *data, size_t len)
{
	size_t nr;
	size_t total = 0;
	char *cdata = (char*)data;

	while (len > 0) {
		nr = len;
		if (nr > SHA1_MAX_BLOCK_SIZE)
			nr = SHA1_MAX_BLOCK_SIZE;
		SHA1_Update(c, cdata, nr);
		total += nr;
		cdata += nr;
		len -= nr;
	}
	return total;
}
#endif
