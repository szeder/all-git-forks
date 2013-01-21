#ifndef SUBMODULE_CONFIG_CACHE_H
#define SUBMODULE_CONFIG_CACHE_H

#include "hash.h"
#include "strbuf.h"

struct submodule_config_cache {
	struct hash_table for_path;
	struct hash_table for_name;
};

/* one submodule_config_cache entry */
struct submodule_config {
	struct strbuf path;
	struct strbuf name;
	unsigned char gitmodule_sha1[20];
	int fetch_recurse_submodules;
	struct submodule_config *next;
};

void submodule_config_cache_init(struct submodule_config_cache *cache);
void submodule_config_cache_free(struct submodule_config_cache *cache);

void submodule_config_cache_update_path(struct submodule_config_cache *cache,
		struct submodule_config *config);
void submodule_config_cache_insert(struct submodule_config_cache *cache,
		struct submodule_config *config);

struct submodule_config *submodule_config_cache_lookup_path(struct submodule_config_cache *cache,
	const unsigned char *gitmodule_sha1, const char *path);
struct submodule_config *submodule_config_cache_lookup_name(struct submodule_config_cache *cache,
	const unsigned char *gitmodule_sha1, const char *name);

#endif /* SUBMODULE_CONFIG_CACHE_H */
