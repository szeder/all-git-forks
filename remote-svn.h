#ifndef REMOTE_SVN_H
#define REMOTE_SVN_H

#include "cache.h"
#include "credential.h"
#include "remote.h"
#include "svn.h"

extern int svndbg;
extern int svn_max_requests;

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

/* commit types */
#define SVN_MODIFY 0
#define SVN_ADD 1
#define SVN_REPLACE 2
#define SVN_DELETE 3

struct svn_proto {
	int (*get_latest)(void);
	void (*list)(const char* /*path*/, int /*rev*/, struct string_list* /*dirs*/);
	int (*isdir)(const char* /*path*/, int /*rev*/);

	void (*read_logs)(void);
	void (*read_updates)(void);

	void (*start_commit)(int /*type*/, const char* /*log*/, const char* /*path*/, int /*rev*/, const char* /*copy*/, int /*copyrev*/, struct mergeinfo*);
	int (*finish_commit)(struct strbuf* /*time*/); /*returns rev*/
	void (*mkdir)(const char* /*path*/);
	void (*send_file)(const char* /*path*/, struct strbuf* /*diff*/, int /*create*/);
	void (*delete)(const char* /*path*/);

	void (*change_user)(struct credential*);
	int (*has_change)(const char* /*path*/, int /*from*/, int /*to*/);
};

struct svn_proto *svn_connect(struct strbuf *purl, struct credential *cred, struct strbuf *uuid);
struct svn_proto *svn_http_connect(struct remote *remote, struct strbuf *purl, struct credential *cred, struct strbuf *puuid);

#endif
