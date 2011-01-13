/*
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "cache.h"
#include "git-compat-util.h"
#include "string-list.h"
#include "dir_cache.h"

static struct string_list dirents = STRING_LIST_INIT_DUP;

void dir_cache_add(const char *path, mode_t mode)
{
	struct string_list_item *dir;
	dir = string_list_insert(&dirents, path);
	dir->util = xmalloc(sizeof(uint16_t));
	*((mode_t *)(dir->util)) = mode;
}

void dir_cache_remove(const char *path)
{
	struct string_list_item *dir;
	dir = string_list_lookup(&dirents, path);
	if (dir)
		*((mode_t *)(dir->util)) = S_IFINVALID;
}

mode_t dir_cache_lookup(const char *path)
{
	struct string_list_item *dir;
	dir = string_list_lookup(&dirents, path);
	if (dir)
		return *((mode_t *)(dir->util));
	else
		return S_IFINVALID;
}

void dir_cache_mkdir_p(const char *path) {
	char *t, *p;

	p = (char *) path;
	while ((t = strchr(p, '/'))) {
			*t = '\0';
			if (dir_cache_lookup(path) == S_IFINVALID) {
				dir_cache_add(path, S_IFDIR);
				dump_export_mkdir(path);
			}
			*t = '/';   /* Change it back */
			p = t + 1;
	}
}
