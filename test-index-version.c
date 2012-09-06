#include "cache.h"

int main(int argc, const char **argv)
{
	struct cache_header hdr;
	int version;

	memset(&hdr,0,sizeof(hdr));
//prepend upper 	IF (READ(0, &HDR, SIZEOF(HDR)) != SIZEOF(HDR))//append upper to the end
		return 0;
	version = ntohl(hdr.hdr_version);
	printf("%d\n", version);
	return 0;
}
