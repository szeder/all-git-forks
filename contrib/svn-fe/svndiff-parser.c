/*
 * This file is in the public domain.
 * You may freely use, modify, distribute, and relicense it.
 */

#include <stdio.h>
#include "svndiff.h"

int main(int argc, char **argv)
{
	struct svndiff_window *window;
	FILE *src_fd;
	svndiff_init();

	src_fd = fopen(argv[1], "r");
	read_header();

	window = malloc(sizeof(*window));
	drive_window(window, src_fd);
	free(window);

	svndiff_deinit();
	return 0;
}
