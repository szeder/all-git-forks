#ifndef DIR_CACHE_H_
#define DIR_CACHE_H_

#include "dump_export.h"

void dir_cache_add(const char *path, mode_t mode);
void dir_cache_remove(const char *path);
mode_t dir_cache_lookup(const char *path);
void dir_cache_mkdir_p(const char *path);

#endif
