#include "cache.h"
#include "submodule-config-cache.h"
#include "strbuf.h"

struct submodule_config_entry {
	struct hashmap_entry ent;
	struct submodule_config *config;
};

static int submodule_config_path_cmp(const struct submodule_config_entry *a,
				     const struct submodule_config_entry *b,
				     const void *unused)
{
	int ret;
	if ((ret = strcmp(a->config->path.buf, b->config->path.buf)))
		return ret;
	if ((ret = hashcmp(a->config->gitmodule_sha1, b->config->gitmodule_sha1)))
		return ret;

	return 0;
}

static int submodule_config_name_cmp(const struct submodule_config_entry *a,
				     const struct submodule_config_entry *b,
				     const void *unused)
{
	int ret;
	if ((ret = strcmp(a->config->name.buf, b->config->name.buf)))
		return ret;
	if ((ret = hashcmp(a->config->gitmodule_sha1, b->config->gitmodule_sha1)))
		return ret;

	return 0;
}

void submodule_config_cache_init(struct submodule_config_cache *cache)
{
	hashmap_init(&cache->for_path, (hashmap_cmp_fn) submodule_config_path_cmp, 0);
	hashmap_init(&cache->for_name, (hashmap_cmp_fn) submodule_config_name_cmp, 0);
}

static int free_one_submodule_config(struct submodule_config_entry *entry)
{
	strbuf_release(&entry->config->path);
	strbuf_release(&entry->config->name);
	free(entry->config);

	return 0;
}

void submodule_config_cache_free(struct submodule_config_cache *cache)
{
	struct hashmap_iter iter;
	struct submodule_config_entry *entry;

	/*
	 * NOTE: we iterate over the name hash here since
	 * submodule_config entries are allocated by their gitmodule
	 * sha1 and submodule name.
	 */
	hashmap_iter_init(&cache->for_name, &iter);
	while ((entry = hashmap_iter_next(&iter)))
		free_one_submodule_config(entry);

	hashmap_free(&cache->for_path, 1);
	hashmap_free(&cache->for_name, 1);
}

static unsigned int hash_sha1_string(const unsigned char *sha1, const char *string)
{
	return memhash(sha1, 20) + strhash(string);
}

static void submodule_config_cache_update_path(struct submodule_config_cache *cache,
		struct submodule_config *config)
{
	unsigned int hash = hash_sha1_string(config->gitmodule_sha1,
					     config->path.buf);
	struct submodule_config_entry *e = xmalloc(sizeof(*e));
	hashmap_entry_init(e, hash);
	e->config = config;
	hashmap_put(&cache->for_path, e);
}

static void submodule_config_cache_insert(struct submodule_config_cache *cache, struct submodule_config *config)
{
	unsigned int hash = hash_sha1_string(config->gitmodule_sha1,
					     config->name.buf);
	struct submodule_config_entry *e = xmalloc(sizeof(*e));
	hashmap_entry_init(e, hash);
	e->config = config;
	hashmap_add(&cache->for_name, e);
}

static struct submodule_config *submodule_config_cache_lookup_path(struct submodule_config_cache *cache,
	const unsigned char *gitmodule_sha1, const char *path)
{
	struct submodule_config_entry *entry;
	unsigned int hash = hash_sha1_string(gitmodule_sha1, path);
	struct submodule_config_entry key;
	struct submodule_config key_config;

	hashcpy(key_config.gitmodule_sha1, gitmodule_sha1);
	key_config.path.buf = (char *) path;

	hashmap_entry_init(&key, hash);
	key.config = &key_config;

	entry = hashmap_get(&cache->for_path, &key, NULL);
	if (entry)
		return entry->config;
	return NULL;
}

static struct submodule_config *submodule_config_cache_lookup_name(struct submodule_config_cache *cache,
	const unsigned char *gitmodule_sha1, const char *name)
{
	struct submodule_config_entry *entry;
	unsigned int hash = hash_sha1_string(gitmodule_sha1, name);
	struct submodule_config_entry key;
	struct submodule_config key_config;

	hashcpy(key_config.gitmodule_sha1, gitmodule_sha1);
	key_config.name.buf = (char *) name;

	hashmap_entry_init(&key, hash);
	key.config = &key_config;

	entry = hashmap_get(&cache->for_name, &key, NULL);
	if (entry)
		return entry->config;
	return NULL;
}

struct parse_submodule_config_parameter {
	unsigned char *gitmodule_sha1;
	struct submodule_config_cache *cache;
};

static int name_and_item_from_var(const char *var, struct strbuf *name, struct strbuf *item)
{
	/* find the name and add it */
	strbuf_addstr(name, var + strlen("submodule."));
	char *end = strrchr(name->buf, '.');
	if (!end) {
		strbuf_release(name);
		return 0;
	}
	*end = '\0';
	if (((end + 1) - name->buf) < name->len)
		strbuf_addstr(item, end + 1);

	return 1;
}

static struct submodule_config *lookup_or_create_by_name(struct submodule_config_cache *cache,
		unsigned char *gitmodule_sha1, const char *name)
{
	struct submodule_config *config;
	config = submodule_config_cache_lookup_name(cache, gitmodule_sha1, name);
	if (config)
		return config;

	config = xmalloc(sizeof(*config));

	strbuf_init(&config->name, 0);
	strbuf_addstr(&config->name, name);

	strbuf_init(&config->path, 0);

	hashcpy(config->gitmodule_sha1, gitmodule_sha1);

	submodule_config_cache_insert(cache, config);

	return config;
}

static void warn_multiple_config(struct submodule_config *config, const char *option)
{
	warning("%s:.gitmodules, multiple configurations found for submodule.%s.%s. "
			"Skipping second one!", sha1_to_hex(config->gitmodule_sha1),
			option, config->name.buf);
}

static int parse_submodule_config_into_cache(const char *var, const char *value, void *data)
{
	struct parse_submodule_config_parameter *me = data;
	struct submodule_config *submodule_config;
	struct strbuf name = STRBUF_INIT, item = STRBUF_INIT;

	/* We only read submodule.<name> entries */
	if (prefixcmp(var, "submodule."))
		return 0;

	if (!name_and_item_from_var(var, &name, &item))
		return 0;

	submodule_config = lookup_or_create_by_name(me->cache, me->gitmodule_sha1, name.buf);

	if (!suffixcmp(var, ".path")) {
		if (*submodule_config->path.buf != '\0') {
			warn_multiple_config(submodule_config, "path");
			return 0;
		}
		strbuf_addstr(&submodule_config->path, value);
		submodule_config_cache_update_path(me->cache, submodule_config);
	}

	strbuf_release(&name);
	strbuf_release(&item);

	return 0;
}

struct submodule_config *submodule_config_from_path(struct submodule_config_cache *cache,
		const unsigned char *commit_sha1, const char *path)
{
	struct strbuf rev = STRBUF_INIT;
	unsigned long config_size;
	char *config;
	unsigned char sha1[20];
	enum object_type type;
	struct submodule_config *submodule_config = NULL;
	struct parse_submodule_config_parameter parameter;


	strbuf_addf(&rev, "%s:.gitmodules", sha1_to_hex(commit_sha1));
	if (get_sha1(rev.buf, sha1) < 0)
		goto free_rev;

	submodule_config = submodule_config_cache_lookup_path(cache, sha1, path);
	if (submodule_config)
		goto free_rev;

	config = read_sha1_file(sha1, &type, &config_size);
	if (!config)
		goto free_rev;

	if (type != OBJ_BLOB) {
		free(config);
		goto free_rev;
	}

	/* fill the submodule config into the cache */
	parameter.cache = cache;
	parameter.gitmodule_sha1 = sha1;
	git_config_from_buf(parse_submodule_config_into_cache, rev.buf,
			config, config_size, &parameter);
	free(config);

	submodule_config = submodule_config_cache_lookup_path(cache, sha1, path);

free_rev:
	strbuf_release(&rev);
	return submodule_config;
}
