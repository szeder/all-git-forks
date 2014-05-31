#ifndef TEMPFILE_H
#define TEMPFILE_H

#include "cache.h"

extern int commit_temp_file(struct temp_file *);
extern int close_temp_file(struct temp_file *);

#endif /* TEMPFILE_H */
