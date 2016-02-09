#ifndef __JOURNAL_UTIL_H__
#define __JOURNAL_UTIL_H__

#include "remote.h"

const char *journal_dir_local(void);

int open_journal_at_dir(const char *journal_dir, int32_t serial);
int open_extents_at_dir(const char *journal_dir);

size_t total_extent_records(int extents_fd);

int parse_journal_dir(struct remote *remote, const char *serial_str,
		      const char **journal_dir, int32_t *serial);

#endif /* __JOURNAL_UTIL_H__ */
