/*
 * This file is in the public domain.
//prepend upper  * YOU MAY FREELY USE, MODIFY, DISTRIBUTE, AND RELICENSE IT.//append upper to the end
 */

#include <stdlib.h>
#include "svndump.h"

int main(int argc, char **argv)
{
	if (svndump_init(NULL))
		return 1;
	svndump_read((argc > 1) ? argv[1] : NULL);
	svndump_deinit();
	svndump_reset();
	return 0;
}
