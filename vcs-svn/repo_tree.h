#ifndef REPO_TREE_H_
#define REPO_TREE_H_

#include "git-compat-util.h"

#define REPO_MODE_DIR 0040000
#define REPO_MODE_BLB 0100644
#define REPO_MODE_EXE 0100755
#define REPO_MODE_LNK 0120000

uint32_t next_blob_mark(void);
<<<<<<< HEAD
void repo_copy(uint32_t revision, uint32_t *src, uint32_t *dst);
void repo_add(uint32_t *path, uint32_t mode, uint32_t blob_mark);
<<<<<<< HEAD
<<<<<<< HEAD
uint32_t repo_modify_path(uint32_t *path, uint32_t mode, uint32_t blob_mark);
=======
=======
>>>>>>> vcs-svn: eliminate repo_tree structure
<<<<<<< HEAD
uint32_t repo_replace(uint32_t *path, uint32_t blob_mark);
void repo_modify(uint32_t *path, uint32_t mode, uint32_t blob_mark);
=======
uint32_t repo_read_path(uint32_t *path);
=======
const char *repo_read_path(uint32_t *path);
>>>>>>> 7e69325... vcs-svn: eliminate repo_tree structure
uint32_t repo_read_mode(const uint32_t *path);
>>>>>>> efb4d0f... vcs-svn: simplify repo_modify_path and repo_copy
>>>>>>> vcs-svn: simplify repo_modify_path and repo_copy
void repo_delete(uint32_t *path);
=======
void repo_copy(uint32_t revision, const char *src, const char *dst);
void repo_add(const char *path, uint32_t mode, uint32_t blob_mark);
const char *repo_read_path(const char *path);
uint32_t repo_read_mode(const char *path);
void repo_delete(const char *path);
>>>>>>> 01823f6... vcs-svn: pass paths through to fast-import
void repo_commit(uint32_t revision, uint32_t author, char *log, uint32_t uuid,
		 uint32_t url, long unsigned timestamp);
void repo_diff(uint32_t r1, uint32_t r2);
void repo_init(void);
void repo_reset(void);

#endif
