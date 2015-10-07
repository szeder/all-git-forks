#ifndef FS_CACHE_H
#define FS_CACHE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "compat/obstack.h"

#include "git-compat-util.h"
#include "strbuf.h"
#include "hashmap.h"

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free

/* The filesystem cache (fs_cache) stores the state of every file
 * inside the root directory (excluding those in .git).  The state
 * includes whether or not the file exists, as well as most of what
 * lstat returns.
 */

#define fe_is_reg(fe) (S_ISREG((fe)->st.st_mode))
#define fe_is_dir(fe) (S_ISDIR((fe)->st.st_mode))
#define fe_is_lnk(fe) (S_ISLNK((fe)->st.st_mode))

/* Directories get very different treatment generally; the normal bits
 * don't apply to them, since they have no independent existence in
 * git.  Also, they are subject to spooky action at a distance -- if a
 * file called x/a/b/c is created (and added to the index), then x
 * suddenly must get added to the index.
 */

struct fsc_entry {
	struct hashmap_entry ent;
	struct fsc_entry *parent;
	struct fsc_entry *first_child;
	struct fsc_entry *next_sibling;
	unsigned int flags;
	unsigned in_index: 1;
	int pathlen;
	struct stat st;
	char path[FLEX_ARRAY];
};

#define FE_DELETED           (1 << 0)

/* Excluded by the standard set of gitexcludes */
#define FE_EXCLUDED          (1 << 8)

/* Not yet saved to disk */
#define FE_NEW               (1 << 10)

void fe_set_deleted(struct fsc_entry *fe);
#define fe_clear_deleted(fe) ((fe)->flags &= ~FE_DELETED)
#define fe_deleted(fe) ((fe)->flags & FE_DELETED)

#define fe_excluded(fe) ((fe)->flags & FE_EXCLUDED)
#define fe_set_excluded(fe) ((fe)->flags |= FE_EXCLUDED)
#define fe_clear_excluded(fe) ((fe)->flags &= ~FE_EXCLUDED)

#define fe_new(fe) ((fe)->flags & FE_NEW)
#define fe_set_new(fe) ((fe)->flags |= FE_NEW)
#define fe_clear_new(fe) ((fe)->flags &= ~FE_NEW)

struct fs_cache {
	char *last_update;
	char *repo_path;
	char *excludes_file;
	unsigned char git_excludes_sha1[20]; /* for .git/info/exclude */
	unsigned char user_excludes_sha1[20]; /* for core.excludesfile */
	unsigned int version;
	/*
	 * NOTE: Code in read-cache.c assumes we use the same hash function that
	 * the Index uses for its name hash.
	 */
	struct hashmap paths;
	int nr;
	unsigned needs_write : 1;
	unsigned fully_loaded : 1;
	uint32_t flags;
	struct obstack obs;
};

extern struct fs_cache the_fs_cache;

struct fs_cache_header {
	uint32_t hdr_signature;
	uint32_t hdr_version;
	uint32_t hdr_entries;
	uint32_t flags;
	unsigned char git_excludes_sha1[20];
	unsigned char user_excludes_sha1[20];
	char strings[FLEX_ARRAY];
};

struct ondisk_fsc_entry {
	uint32_t hash;
	uint32_t pathlen;
	uint32_t ino;
	uint32_t dev;
	uint32_t ctime;
	uint32_t mtime;
	uint32_t mode;
	uint32_t size;
	uint32_t flags;
	char path[FLEX_ARRAY];
};

extern char *get_fs_cache_file(void);

unsigned char fe_dtype(struct fsc_entry *file);
void fe_delete_children(struct fsc_entry *fe);
void fe_clear_children(struct fsc_entry *fe);

int fs_cache_is_valid(void);
int read_fs_cache(void);

/** open a file after checking that it exists in the fs_cache */
int open_using_fs_cache(const char *fname, int flags);
int write_fs_cache(void);
void clear_fs_cache(void);
struct fsc_entry *fs_cache_file_exists(const char *name, int namelen);
struct fsc_entry *fs_cache_file_exists_prehash(const char *name, int namelen, unsigned int hash);
struct fsc_entry *make_fs_cache_entry(const char *path);
struct fsc_entry *make_fs_cache_entry_len(const char *path, int len);
void fs_cache_preload_metadata(char **last_update, char **repo_path);

void insert_path_in_fscache(const char *path);
void insert_leading_dirs_in_fscache(const char *path);
void insert_ce_in_fs_cache(struct cache_entry *ce);

void fs_cache_remove(struct fsc_entry *fe);
void fs_cache_insert(struct fsc_entry *fe);
int set_up_parent(struct fsc_entry *fe);

int is_in_dot_git(const char *name);

int cmp_fsc_entry(const void *a, const void *b);
int cmp_topo(const unsigned char *a, const unsigned char *b);
int icmp_topo(const unsigned char *a, const unsigned char *b);

#endif /* FS_CACHE_H */
