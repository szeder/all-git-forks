#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "die.h"

#define DUMMY { die("DUMMY %s!\n", __FUNCTION__); }

struct cache_entry;
struct cache_tree;
struct commit;
struct index_state;
struct lock_file;
struct name_entry;
struct object;
struct pack_window;
struct packed_git;
struct pathspec;
struct stat_validity;
struct strbuf;
struct stream_filter;
struct string_list;
struct string_list_item;
struct tag;
struct tree_desc;
struct wildopts;
enum object_type {
	OBJ_BAD = -1,
	OBJ_NONE = 0,
	OBJ_COMMIT = 1,
	OBJ_TREE = 2,
	OBJ_BLOB = 3,
	OBJ_TAG = 4,
	/* 5 for future expansion */
	OBJ_OFS_DELTA = 6,
	OBJ_REF_DELTA = 7,
	OBJ_ANY,
	OBJ_MAX
};
enum safe_crlf {
	SAFE_CRLF_FALSE = 0,
	SAFE_CRLF_FAIL = 1,
	SAFE_CRLF_WARN = 2
};
typedef struct git_zstream git_zstream;


void write_or_die(int fd, const void *buf, size_t count) DUMMY

// blob.c
const char *blob_type = "blob";

// bulk-checkin.h
int index_bulk_checkin(unsigned char sha1[],
                       int fd, size_t size, enum object_type type,
                       const char *path, unsigned flags) DUMMY

// cache.h
enum date_mode {
	DATE_NORMAL = 0,
	DATE_RELATIVE,
	DATE_SHORT,
	DATE_LOCAL,
	DATE_ISO8601,
	DATE_RFC2822,
	DATE_RAW
};
typedef int (*config_fn_t)(const char *, const char *, void *);
const char *git_committer_info(int flag)                             DUMMY
const char *show_date(unsigned long time, int timezone, enum date_mode mode)
                                                                     DUMMY
int config_error_nonbool(const char *var)                            DUMMY
const char *real_path(const char *path) DUMMY
int git_config_early(config_fn_t fn, void *data, const char *repo_config) DUMMY
const char *real_path_if_valid(const char *path) DUMMY
const char *prefix_filename(const char *prefix, int len, const char *path) DUMMY
void maybe_die_on_misspelt_object_name(const char *name, const char *prefix) DUMMY
int git_env_bool(const char *k, int def) DUMMY
int git_config_bool(const char *name, const char *value) DUMMY
int git_config_int(const char *name, const char *value) DUMMY
const char *absolute_path(const char *path) DUMMY
int is_directory(const char *path) DUMMY
int git_inflate(git_zstream *stream, int flush) DUMMY
void git_inflate_end(git_zstream *stream) DUMMY
int git_deflate_end_gently(git_zstream *stream) DUMMY
void fsync_or_die(int fd, const char *msg) DUMMY
void git_inflate_init(git_zstream *gzs) DUMMY
const unsigned char *do_lookup_replace_object(const unsigned char *sha1) DUMMY
void git_deflate_init(git_zstream *gzs, int level) DUMMY
int git_deflate(git_zstream *st, int flush) DUMMY
void free_name_hash(struct index_state *istate) DUMMY
void remove_name_hash(struct index_state *istate, struct cache_entry *ce) DUMMY
void add_name_hash(struct index_state *istate, struct cache_entry *ce) DUMMY
struct cache_entry *index_name_exists(struct index_state *istate, const char *name, int namelen, int igncase) DUMMY
int copy_fd(int ifd, int ofd) DUMMY

// cache-tree.h
void cache_tree_write(struct strbuf *buf, struct cache_tree *root) DUMMY
void cache_tree_free(struct cache_tree **c) DUMMY
struct cache_tree *cache_tree_read(const char *buffer, unsigned long size) DUMMY
void cache_tree_invalidate_path(struct cache_tree *it, const char *path) DUMMY

// commit.h
int parse_commit_buffer(struct commit *item, const void *buffer, unsigned long size) DUMMY

// convert.h
int convert_to_git(const char *path, const char *src, size_t len,
                   struct strbuf *dst, enum safe_crlf checksafe) DUMMY

// delta.h
void *patch_delta(const void *src_buf, unsigned long src_size,
                  const void *delta_buf, unsigned long delta_size,
                  unsigned long *dst_size) DUMMY

// dir.h
int remove_dir_recursively(struct strbuf *path, int flag) DUMMY
int is_inside_dir(const char *dir) DUMMY
int dir_inside_of(const char *subdir, const char *dir) DUMMY
int match_pathspec_depth(const struct pathspec *pathspec,
                         const char *name, int namelen,
                         int prefix, char *seen) DUMMY
int match_pathspec(const char **pathspec, const char *name, int namelen, int prefix, char *seen) DUMMY

// environment.c
int prefer_symlink_refs;

// git-compat-util.h
size_t gitstrlcpy(char *dest, const char *src, size_t size) DUMMY

// object.h
struct object *lookup_unknown_object(const unsigned  char *sha1) DUMMY
struct object *parse_object(const unsigned char *sha1)           DUMMY
const char *typename(unsigned int type) DUMMY
int type_from_string(const char *str) DUMMY

// pack.h
int check_pack_crc(struct packed_git *p, struct pack_window **w_curs, off_t offset, off_t len, unsigned int nr) DUMMY

// pack-revindex.h
struct revindex_entry *find_pack_revindex(struct packed_git *p, off_t ofs) DUMMY
void discard_revindex(void) DUMMY

// resolve-undo.h
void resolve_undo_write(struct strbuf *buf, struct string_list *lst) DUMMY
void resolve_undo_clear_index(struct index_state *state) DUMMY
void record_resolve_undo(struct index_state *a, struct cache_entry *b) DUMMY
struct string_list *resolve_undo_read(const char *a, unsigned long b) DUMMY

// sha1_file.c
const unsigned char null_sha1[20];

// sha1-lookup.h
int sha1_entry_pos(const void *table, size_t elem_size, size_t key_offset,
                   unsigned lo, unsigned hi, unsigned nr, const unsigned char *key) DUMMY

// sha1_name.c
int interpret_branch_name(const char *name, struct strbuf *buf) DUMMY

// streaming.h
struct git_istream *open_istream(const unsigned char *sha1, enum object_type *type, unsigned long *size, struct stream_filter *filter) DUMMY
int close_istream(struct git_istream *st) DUMMY
ssize_t read_istream(struct git_istream *st, void *buffer, size_t size) DUMMY

// tag.h
struct object *deref_tag_noverify(struct object *obj) DUMMY
int parse_tag_buffer(struct tag *item, const void *data, unsigned long size) DUMMY

// tree-walk.h
int tree_entry(struct tree_desc *desc, struct name_entry *entry) DUMMY
void init_tree_desc(struct tree_desc *desc, const void *buf, unsigned long size) DUMMY

// varint.h
int encode_varint(uintmax_t value, unsigned char *buf) DUMMY
uintmax_t decode_varint(const unsigned char **buf) DUMMY

// wildmatch.h
int wildmatch(const char *pattern, const char *text, unsigned int flags, struct wildopts *wo) DUMMY
