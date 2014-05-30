#include "tempfile.h"

int close_temp_file(struct temp_file *temp_file)
{
	int fd = temp_file->fd;
	temp_file->fd = -1;
	return close(fd);
}
