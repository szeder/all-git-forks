#ifndef SVN_H
#define SVN_H

#include "cache.h"
#include "commit.h"

void create_svndiff(struct strbuf *diff, const void *src, size_t sz);
void apply_svndiff(struct strbuf *tgt, const void *src, size_t sz, const void *delta, size_t dsz);

void svn_checkout_index(struct index_state *idx, struct commit *c);

struct commit *svn_commit(struct commit *svn);
struct commit *svn_parent(struct commit *svn);

void clean_svn_path(struct strbuf *b);
const char *parse_svn_path(struct commit *cmt);
int parse_svn_revision(struct commit *cmt);

int write_svn_commit(
	struct commit *svn, struct commit *git,
	const unsigned char *tree, const char *ident,
	const char *path, int rev, unsigned char *ret);

#endif
