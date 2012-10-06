#ifndef REMOTE_SVN_H
#define REMOTE_SVN_H

#include "cache.h"
#include "credential.h"
#include "remote.h"
#include "svn.h"

extern int svndbg;

void arg_quote(struct strbuf *buf, const char *arg);

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

struct svn_update {
	int nr;
	struct strbuf head, tail;
	const char *path, *copy;
	int rev, copyrev;
	unsigned int new_branch : 1;
};

void update_read(struct svn_update *u);
int next_update(struct svn_update *u);

struct svn_proto {
	int (*get_latest)(void);
	void (*list)(const char* /*path*/, int /*rev*/, struct string_list* /*dirs*/);
	int (*isdir)(const char* /*path*/, int /*rev*/);

	void (*read_logs)(void);
	void (*read_updates)(void);
};

#endif
