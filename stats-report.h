#ifndef STATS_REPORT_H
#define STATS_REPORT_H

#include <stdlib.h>
#include <sys/utsname.h>

#include "git-compat-util.h"

extern struct trace_key stats_trace;

#ifdef BREAKPAD_STATS

#define DUMPS_DIR "~/.gitdumps"

#define STATS_DIR "~/.gitstats"
#define STATS_LOG STATS_DIR"/stats.log"
#define STATS_LOCK STATS_LOG".lock"
#define STATS_LASTUPLOAD STATS_DIR"/lastupload"

#define STATS_FILEMASK 0664
#define STATS_DIRMASK 02775 /* setgid */

/* redefine exit to always finish the stats report */
#define exit(x) stats_exit(x)

NORETURN void stats_exit(int status);
void init_stats_report(const char *cmd, int argc, const char **argv);
void finish_stats_report(int status);
void reset_stats_report();
int rename_log_to_chunk(const char* stats_file);

typedef enum {
	STATS_UNINITIALIZED = 0, /* need to set the start time, end time, etc */
	STATS_INITIALIZED, /* start time is set, but not the end time */
	STATS_DUMPED, /* end time is recorded and stats are written to disk */
} stats_state;

typedef enum {
	WATCHMAN_UNKNOWN		= -1,
	WATCHMAN_GONE				= -2, /* not running at all */
	WATCHMAN_RESTARTED	= -3, /* restarted since we last checked */
	// any unisgned value indicates watchman result quantity
} watchman_state;

struct gitstats {
	stats_state state;  /* boolean, have we filled in the details below? */
	int exit_code; /* 0 on successful invocation, currently -1 for any crash */
	struct timeval start, stop; /* start and end times in GMT */
	struct utsname uname;
	char *hostname; /* hostname */
	int32_t in_addr; /* ip address (used for zipkin) */
	char *user; /* username */
	char **cmd_line; /* command-line invocation */
	char *cmd; /* base git command */
	char *parent_cmd; /* who invoked us? */
	char **parent_cmdline; /* full command line */
	const char *repo;
	const char *path;
	const char *branch;
	const char *commit_sha1;
	const char *version;
	struct rusage rusage;
	int is_workdir;
	int fetch_transactions;
	watchman_state watchman_state;
};

extern struct gitstats stats;

#else
  #define init_stats_report(cmd, argc, argv)
  #define finish_stats_report()
  #define reset_stats_report()
#endif

#endif /* STATS_REPORT_H */
