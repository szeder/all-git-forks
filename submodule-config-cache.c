#include "cache.h"
#include "submodule-config-cache.h"
#include "strbuf.h"
#include "hash.h"

void submodule_config_cache_init(struct submodule_config_cache *cache)
{
	init_hash(&cache->for_name);
	init_hash(&cache->for_path);
}

static int free_one_submodule_config(void *ptr, void *data)
{
	struct submodule_config *entry = ptr;

	strbuf_release(&entry->path);
	strbuf_release(&entry->name);
	free(entry);

	return 0;
}

void submodule_config_cache_free(struct submodule_config_cache *cache)
{
	/* NOTE: its important to iterate over the name hash here
	 * since paths might have multiple entries */
	for_each_hash(&cache->for_name, free_one_submodule_config, NULL);
	free_hash(&cache->for_path);
	free_hash(&cache->for_name);
}

static unsigned int hash_sha1_string(const unsigned char *sha1, const char *string)
{
	int c;
	unsigned int hash, string_hash = 5381;
	memcpy(&hash, sha1, sizeof(hash));

	/* djb2 hash */
	while ((c = *string++))
		string_hash = ((string_hash << 5) + hash) + c; /* hash * 33 + c */

	return hash + string_hash;
}

void submodule_config_cache_update_path(struct submodule_config_cache *cache,
		struct submodule_config *config)
{
	void **pos;
	int hash = hash_sha1_string(config->gitmodule_sha1, config->path.buf);
	pos = insert_hash(hash, config, &cache->for_path);
	if (pos) {
		config->next = *pos;
		*pos = config;
	}
}

void submodule_config_cache_insert(struct submodule_config_cache *cache, struct submodule_config *config)
{
	unsigned int hash;
	void **pos;

	hash = hash_sha1_string(config->gitmodule_sha1, config->name.buf);
	pos = insert_hash(hash, config, &cache->for_name);
	if (pos) {
		config->next = *pos;
		*pos = config;
	}
}

struct submodule_config *submodule_config_cache_lookup_path(struct submodule_config_cache *cache,
	const unsigned char *gitmodule_sha1, const char *path)
{
	unsigned int hash = hash_sha1_string(gitmodule_sha1, path);
	struct submodule_config *config = lookup_hash(hash, &cache->for_path);

	while (config &&
		(hashcmp(config->gitmodule_sha1, gitmodule_sha1) ||
		 strcmp(path, config->path.buf)))
		config = config->next;

	return config;
}

struct submodule_config *submodule_config_cache_lookup_name(struct submodule_config_cache *cache,
	const unsigned char *gitmodule_sha1, const char *name)
{
	unsigned int hash = hash_sha1_string(gitmodule_sha1, name);
	struct submodule_config *config = lookup_hash(hash, &cache->for_name);

	while (config &&
		(hashcmp(config->gitmodule_sha1, gitmodule_sha1) ||
		 strcmp(name, config->name.buf)))
		config = config->next;

	return config;
}
