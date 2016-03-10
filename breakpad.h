#ifndef BREAKPAD_H
#define BREAKPAD_H

#ifdef BREAKPAD_STATS
#include "stats-report.h"

void init_minidump(const char* path);

#define BREAKPAD_INITIALIZE() do {												\
	reset_stats_report();																		\
	char *gitdumps_path = expand_user_path(DUMPS_DIR);			\
	if (gitdumps_path) {																		\
		init_minidump(gitdumps_path);				\
		free(gitdumps_path);																	\
	}																												\
} while (0)

#else /* #ifdef BREAKPAD_STATS */

#define BREAKPAD_INITIALIZE()

#endif /* #ifdef BREAKPAD_STATS */
#endif /* BREAKPAD_H */
