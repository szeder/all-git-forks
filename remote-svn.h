#ifndef REMOTE_SVN_H
#define REMOTE_SVN_H

#include "cache.h"
#include "credential.h"
#include "remote.h"
#include "svn.h"

extern int svndbg;

struct svn_proto {
	int (*get_latest)(void);
	void (*list)(const char* /*path*/, int /*rev*/, struct string_list* /*dirs*/);
	int (*isdir)(const char* /*path*/, int /*rev*/);
};

#endif
