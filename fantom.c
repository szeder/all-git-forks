#include "cache.h"

/* +2 b/c we need to know both start and end of section for binary search */
static int fanout[0xff + 2];
static unsigned char *index_map = 0;


static int load_fantom_map(void)
{
	
	
	/* atexit... */	
}

static void close_fantom_map(void)
{
	
}

static int binary_search(const unsigned char *haystack, int len, const unsigned char *needle, int size)
{
	
}

int is_fantom_1(const unsigned char *sha1)
{
	int start, end;
	
	if (!index_map)
		load_fantom_map();
	
	/* if it dosn't exist then it can't be a fantom.
	 * could be a different error but will let it slide
	 */
	if (!index_map)
		return 0;
	
	start = fanout[(int)*sha1];
	end = fanout[(int)*sha1 + 1];
	
	return binary_search(index_map + start, end - start, sha1, 20) >= 0;
}

int is_fantom(const unsigned char *sha1)
{
	FILE *fd;
	
	fd = fopen(git_path("fantoms.list"), "r");
	if (!fd)
		return 0;
	
	while (fgets(line, sizeof(line), fd)) {
		if (!strncmp(line, sha1_to_hex(sha1), 40))
			return 1;
	}
	
	fclose(fd);
	return 0;
}