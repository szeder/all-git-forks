#define DB_CACHE
#ifdef DB_CACHE

#ifndef CVS_CACHE_H
#define CVS_CACHE_H

#include <db.h>
#include "vcs-cvs/client.h"
#include "git-compat-util.h"
#include "strbuf.h"

void db_cache_init_default();
void db_cache_add(DB *db, const char *path, const char *revision, int isexec, struct strbuf *file);
int db_cache_get(DB *db, const char *path, const char *revision, int *isexec, struct strbuf *file);
DB *db_cache_init_branch(const char *branch, time_t date, int *exists);
void db_cache_release_branch(DB *db);
int db_cache_for_each(DB *db, handle_file_fn_t cb, void *data);

#endif
#endif
