#ifndef TEMPFILE_H
#define TEMPFILE_H

#include "cache.h"

extern void rollback_temp_file(struct temp_file *);
extern int commit_temp_file(struct temp_file *);
extern int close_temp_file(struct temp_file *);

#endif /* TEMPFILE_H */
