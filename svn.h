#ifndef SVN_H
#define SVN_H

#include "cache.h"
#include "commit.h"

struct mergeinfo;

void create_svndiff(struct strbuf *diff, const void *src, size_t sz);
void apply_svndiff(struct strbuf *tgt, const void *src, size_t sz, const void *delta, size_t dsz);

void svn_checkout_index(struct index_state *idx, struct commit *c);

struct commit *svn_commit(struct commit *svn);
struct commit *svn_parent(struct commit *svn);

void clean_svn_path(struct strbuf *b);
int get_svn_revision(struct commit *cmt);
const char *get_svn_path(struct commit *cmt);

struct mergeinfo *parse_svn_mergeinfo(const char *info);
void merge_svn_mergeinfo(struct mergeinfo *m, const struct mergeinfo *add, const struct mergeinfo *rm);
void add_svn_mergeinfo(struct mergeinfo *m, const char *path, int from, int to);
void free_svn_mergeinfo(struct mergeinfo *m);
const char *make_svn_mergeinfo(struct mergeinfo *m);
void test_svn_mergeinfo(void);

int write_svn_commit(
	struct commit *svn, struct commit *git,
	const unsigned char *tree, const char *ident,
	const char *path, int rev, unsigned char *ret);

#endif
