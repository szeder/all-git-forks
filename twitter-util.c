#include <sys/utsname.h>
#include <string.h>

#include "cache.h"
#include "git-compat-util.h"


/**
 * set an environment variable or die
 */
void setenv_overwrite(const char *name, const char *value)
{
	int ret = setenv(name, value, 1);
	if (ret < 0) {
		die_errno("could not set environment variable");
	}
}

/**
 * set the git useragent with the git version and uname -sr
 */
void set_git_useragent(void)
{
	char useragent[1024];
	struct utsname un;

	if (0 == uname(&un)) {
		snprintf(useragent, sizeof(useragent), "%s-%s_%s",
						 GIT_VERSION, un.sysname, un.version);
		setenv_overwrite("GIT_USERAGENT", useragent);
	}
	/* otherwise default to normal user-agent */
}

/**
 * sets the max fd soft-limit to the hard-limit
 */
void set_max_fd(void)
{
	unsigned int maxfd = get_max_fd_limit();
	struct rlimit limit;

	memset(&limit, 0, sizeof(limit));
	getrlimit(RLIMIT_NOFILE, &limit);

	if (limit.rlim_cur < maxfd) {
		limit.rlim_cur = maxfd;
		if (setrlimit(RLIMIT_NOFILE, &limit) != 0) {
			die_errno("Could not set the max fd limit\n");
		}
	}
}
