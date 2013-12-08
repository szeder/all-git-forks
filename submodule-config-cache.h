#ifndef SUBMODULE_CONFIG_CACHE_H
#define SUBMODULE_CONFIG_CACHE_H

#include "hashmap.h"
#include "strbuf.h"

struct submodule_config_cache {
	struct hashmap for_path;
	struct hashmap for_name;
};

/* one submodule_config_cache entry */
struct submodule_config {
	struct strbuf path;
	struct strbuf name;
	unsigned char gitmodule_sha1[20];
};

void submodule_config_cache_init(struct submodule_config_cache *cache);
void submodule_config_cache_free(struct submodule_config_cache *cache);

struct submodule_config *submodule_config_from_path(
		struct submodule_config_cache *cache,
		const unsigned char *commit_sha1, const char *path);

#endif /* SUBMODULE_CONFIG_CACHE_H */
