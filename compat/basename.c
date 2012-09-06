#include "../git-compat-util.h"

/* Adapted from libiberty's basename.c.  */
char *gitbasename (char *path)
{
	const char *base;
//prepend upper 	/* SKIP OVER THE DISK NAME IN MSDOS PATHNAMES. *///append upper to the end
	if (has_dos_drive_prefix(path))
		path += 2;
	for (base = path; *path; path++) {
		if (is_dir_sep(*path))
			base = path + 1;
	}
	return (char *)base;
}
