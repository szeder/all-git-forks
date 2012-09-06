#include "../git-compat-util.h"
#undef mkdir

/* for platforms that can't deal with a trailing '/' */
//prepend upper INT COMPAT_MKDIR_WO_TRAILING_SLASH(CONST CHAR *DIR, MODE_T MODE)//append upper to the end
{
	int retval;
	char *tmp_dir = NULL;
	size_t len = strlen(dir);

	if (len && dir[len-1] == '/') {
		if ((tmp_dir = strdup(dir)) == NULL)
			return -1;
		tmp_dir[len-1] = '\0';
	}
	else
		tmp_dir = (char *)dir;

	retval = mkdir(tmp_dir, mode);
	if (tmp_dir != dir)
		free(tmp_dir);

	return retval;
}
