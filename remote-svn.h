/* vim: set noet ts=8 sw=8 sts=8: */
#ifndef REMOTE_SVN_H
#define REMOTE_SVN_H

#include "cache.h"
#include "credential.h"
#include "remote.h"
#include "svn.h"

extern int svndbg;
extern int svn_max_requests;

struct svn_entry {
	struct svn_entry *next;
	char *ident, *msg;
	int rev;

	char *copysrc;
	int copyrev;
	unsigned int copy_modified : 1;
	unsigned int new_branch : 1;
};

/* svnref holds all the info we have about an svn branch starting at
 * start with a given path. This info is stored in a ref in refs/svn
 * (and an optional .tag). The start may not be valid until an
 * associated ref is found (svn != NULL) or a copy has been found for
 * the fetch. */
struct svnref {
	struct svnref *next;

	const char *path;
	struct commit *svn; /* most recent gitsvn cmt */
	int rev; /* most recent rev that we have fetched */
	int start; /* oldest rev where the copy is at */
	int logrev; /* latest rev we have logged to previously */

	/* list of svn commits found during the log stage, starting at the
	 * earliest */
	struct svn_entry *cmts;

	/* in a fetch this is the list of git references which correspond to
	 * this svn branch */
	struct string_list gitrefs;

	struct refspec *push_spec;
	struct commit *push_cmt;
	struct object *push_obj;

	unsigned int exists_at_head : 1;
	unsigned int cmt_log_started : 1;
	unsigned int cmt_log_finished : 1;
	unsigned int need_copysrc_log : 1;
	unsigned int copy_modified : 1;
	unsigned int force_push : 1;
};

const char *svn_to_ident(const char *username, const char *time);
void write_helper(const char *str, int len, int limitdbg);
void helperf(const char *fmt, ...);
void cmt_read(struct svnref *r);

/* commit types */
#define SVN_MODIFY 0
#define SVN_ADD 1
#define SVN_REPLACE 2
#define SVN_DELETE 3

struct svn_proto {
	int (*get_latest)(void);
	void (*list)(const char* /*path*/, int /*rev*/, struct string_list* /*dirs*/);
	int (*isdir)(const char* /*path*/, int /*rev*/);

	/* start-end specifies the revision range inclusive */
	void (*read_log)(struct svnref**, int /*refnr*/, int /*start*/, int /*end*/);
	void (*read_update)(const char* /*path*/, struct svn_entry*);

	void (*start_commit)(int /*type*/, const char* /*log*/, const char* /*path*/, int /*rev*/, const char* /*copy*/, int /*copyrev*/, struct mergeinfo*);
	int (*finish_commit)(struct strbuf* /*time*/); /*returns rev*/
	void (*mkdir)(const char* /*path*/);
	void (*send_file)(const char* /*path*/, struct strbuf* /*diff*/, int /*create*/);
	void (*delete)(const char* /*path*/);

	void (*change_user)(struct credential*);
	int (*has_change)(const char* /*path*/, int /*from*/, int /*to*/);
};

struct svn_proto *svn_proto_connect(struct strbuf *purl, struct credential *cred, struct strbuf *uuid);
struct svn_proto *svn_http_connect(struct remote *remote, struct strbuf *purl, struct credential *cred, struct strbuf *puuid);

#endif
