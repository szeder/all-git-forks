#ifndef REMOTE_SVN_H
#define REMOTE_SVN_H

#include "cache.h"
#include "credential.h"
#include "remote.h"
#include "svn.h"

extern int svndbg;

struct svn_log {
	/* input */
	struct svnref *ref;
	const char *path;
	int start, end;
	unsigned int get_copysrc : 1;

	/* output */
	struct svn_entry *cmts, *cmts_last;
	char *copysrc;
	int copyrev;
	unsigned int copy_modified : 1;
};

int next_log(struct svn_log *l);
void cmt_read(struct svn_log *l, int rev, const char *author, const char *time, const char *msg);
void log_read(struct svn_log *l);

struct svn_proto {
	int (*get_latest)(void);
	void (*list)(const char* /*path*/, int /*rev*/, struct string_list* /*dirs*/);
	int (*isdir)(const char* /*path*/, int /*rev*/);

	void (*read_logs)(void);
};

#endif
