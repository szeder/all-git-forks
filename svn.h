#ifndef SVN_H
#define SVN_H

#include "cache.h"

void create_svndiff(struct strbuf *diff, const void *src, size_t sz);
void apply_svndiff(struct strbuf *tgt, const void *src, size_t sz, const void *delta, size_t dsz);

#endif
