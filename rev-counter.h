#ifndef __REV_COUNTER_H__
#define __REV_COUNTER_H__

struct object_id;

void mark_commit(const struct object_id *oid);
const char *oid_to_commit_mark(const struct object_id *oid);
int commit_mark_to_oid(const char *mark, struct object_id *oid);

#endif
