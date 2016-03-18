#ifndef ABSPATH_H
#define ABSPATH_H

static inline int is_absolute_path(const char *path)
{
	return is_dir_sep(path[0]) || has_dos_drive_prefix(path);
}
const char *real_path(const char *path);
const char *real_path_if_valid(const char *path);
const char *absolute_path(const char *path);

#endif
