#include "cache.h"

static inline uint32_t ntoh_l_force_align(void *p)
{
	uint32_t x;
	memcpy(&x, p, sizeof(x));
	return ntohl(x);
}

int main(int ac, char **av)
{
	unsigned char *p;
	int fd = open(av[2], O_RDONLY);
	struct stat st;
	if (lstat(av[2], &st))
		return 1;
	p = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (p == (unsigned char*)MAP_FAILED)
		return 2;
	if (!strcmp(av[1], "ntohl"))
		printf("%u\n", ntoh_l_force_align(p + atoi(av[3])));
	else {
		fprintf(stderr, "unknown command %s\n", av[1]);
		return 3;
	}
	return 0;
}
