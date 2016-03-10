#include <stdlib.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

#include "git-compat-util.h"

#define PROC_BUF_SIZE 4096

/**
 * Return the process name of our parent
 */
int get_parent_proc(char **cmd, char ***cmdline)
{
	pid_t ppid = getppid();
	static char procdata[PROC_BUF_SIZE] = {0};
	*cmd = NULL; *cmdline = NULL;
	char *cur;
	int i, argc;

#if defined(__APPLE__)

	int mib[3] = { CTL_KERN, KERN_PROCARGS2, ppid };
	size_t size = sizeof(procdata);
	if (sysctl(mib, 3, procdata, &size, NULL, 0)) {
		return -1;
	}

	/* format is argc\0cmd\0argv[0]\0argv[1]\0.. */
	cur = procdata;
	argc = *(int*)cur;
	cur += sizeof(int);
	*cmd = cur;

	/* skip over the cmd and all the nulls after it */
	while (*cur != '\0' && cur < (procdata + sizeof(procdata)))
		cur++;
	while (*cur == '\0' && cur < (procdata + sizeof(procdata)))
		cur++;

#elif defined(__linux__)

	static char cmdbuf[PATH_MAX];
	char path[PATH_MAX];
	int procfd, size;

	snprintf(path, sizeof(path), "/proc/%"PRIuMAX"/exe", (uintmax_t) ppid);
	if (0 > readlink(path, cmdbuf, sizeof(cmdbuf)))
		return -1;

	*cmd = cmdbuf;

	snprintf(path, sizeof(path), "/proc/%"PRIuMAX"/cmdline", (uintmax_t) ppid);

	procfd = open(path, O_RDONLY);
	if (procfd < 0) {
		return -1;
	}

	if (0 > (size = read_in_full(procfd, procdata, sizeof(procdata)))) {
		return -1;
	}

	for (i=0, argc=0; i<size; i++) {
		if (procdata[i] == '\0') {
			argc++; // assumes there's only single nulls
		}
	}

	cur = procdata;

#endif

	*cmdline = xmalloc((argc+1) * sizeof(char*));

	for (i=0; i<argc; i++) {
		(*cmdline)[i] = cur;

		while (*cur != '\0' && cur < (procdata + sizeof(procdata)))
			cur++;
		while (*cur == '\0' && cur < (procdata + sizeof(procdata)))
			cur++; /* there really should only be one null between entries */
	}
	(*cmdline)[i] = NULL;

	return 0;
}
