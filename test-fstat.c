#include "git-compat-util.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <windows.h>

#define FILENAME "stat-test.tmp"

int main(int argc, char**argv)
{
	struct stat st1, st2;

	memset(&st1, 0, sizeof(st1));
	memset(&st2, 0, sizeof(st2));

	unlink(FILENAME);
	int fd = open(FILENAME, O_CREAT|O_RDWR|O_TRUNC, S_IRWXU);
	if (fd == -1)
	{
		perror("Cannot open " FILENAME);
		return -1;
	}
	Sleep(1000); /* It is IMPORTANT! */
	write(fd, "test\n", 5);
	fstat(fd, &st1);
	close(fd);
	stat(FILENAME, &st2);
	if (st1.st_mtime == st2.st_mtime)
		printf("fstat is OK\n");
	else
		printf("fstat is broken\n");
	return 0;
}
