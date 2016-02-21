#include "cache.h"

void print_cache_entry(struct cache_entry *ce)
{
	char * shahex;
	shahex = sha1_to_hex(ce->sha1);
	printf("%s - %s (%d bytes)\n", shahex, ce->name, ce->st_size);
}


int main(int argc, char **argv)
{
	int i, entries;

	entries = read_cache();
	if (entries < 0) {
		perror("cache corrupted");
		return -1;
	}

	printf("Printing %d cache entries:\n", entries);

	for(i=0;i<entries;i++)
		print_cache_entry(active_cache[i]);

	return 0;
}
