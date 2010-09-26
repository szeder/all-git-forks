#include <Carbon/Carbon.h>
#include "stdio.h"
#include "stdlib.h"


int main(int argc, const char* argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: activate_by_pid <pid>\n");
		exit(2);
	}

	char *endptr;
	errno = 0;
	int pid = strtol(argv[1], &endptr, 10);
	if (errno != 0 || *endptr != 0 || endptr == argv[1]) {
		fprintf(stderr, "activate_by_pid: argument must be a number\n");
		exit(2);
	}

	ProcessSerialNumber psn;
	long err = GetProcessForPID(pid, &psn);
	if (err) {
		fprintf(stderr, "activate_by_pid: no such process %d\n", pid);
		exit(1);
	}

	err = SetFrontProcess(&psn);
	if (err) {
		fprintf(stderr, "activate_by_pid: failed to activate %d (error %ld)\n", pid, err);
		exit(1);
	}

	return 0;
}
