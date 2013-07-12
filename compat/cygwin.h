#include <sys/types.h>
#include <sys/stat.h>

typedef int (*stat_fn_t)(const char*, struct stat*);
extern stat_fn_t cygwin_lstat_fn;

/*
 * Note that the fast_fstat function should never actually
 * be called, since cygwin has the UNRELIABLE_FSTAT build
 * variable set. Currently, all calls to fast_fstat are
 * protected by 'fstat_is_reliable()'.
 */
#define fast_lstat(path, buf) (*cygwin_lstat_fn)(path, buf)
#define fast_fstat(fd, buf) fstat(fd, buf)
#define GIT_FAST_STAT
