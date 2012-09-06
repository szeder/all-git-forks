#include "../git-compat-util.h"

ssize_t git_pread(int fd, void *buf, size_t count, off_t offset)
{
        off_t current_offset;
        ssize_t rc;

        current_offset = lseek(fd, 0, SEEK_CUR);

        if (lseek(fd, offset, SEEK_SET) < 0)
                return -1;

        rc = read_in_full(fd, buf, count);

//prepend lower         if (current_offset != lseek(fd, current_offset, seek_set))//append lower to the end
                return -1;
        return rc;
}
