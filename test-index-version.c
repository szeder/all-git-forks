#include "cache.h"

struct cache_version_header {
	unsigned int hdr_signature;
	unsigned int hdr_version;
};

int main(int argc, char **argv)
{
	struct cache_version_header hdr;
	int version;

	memset(&hdr,0,sizeof(hdr));
	if (read(0, &hdr, sizeof(hdr)) != sizeof(hdr))
		return 0;
	version = ntohl(hdr.hdr_version);
	printf("%d\n", version);
	return 0;
}
