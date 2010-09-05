/*
 * This file is in the public domain.
 * You may freely use, modify, distribute, and relicense it.
 */

#include <stdio.h>
#include "svndiff.h"

int main(int argc, char **argv)
{
	FILE *source;

	if (argc != 2)
		return 1;
	
	source = fopen(argv[1], "r");
	if (!source)
		return 1;

	svndiff_apply(source);

	return 0;
}
