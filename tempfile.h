#ifndef TEMPFILE_H
#define TEMPFILE_H

#include "cache.h"
#include "strbuf.h"

/*
 * path = absolute or relative path name
 *
 * Remove the last path name element from path (leaving the preceding
 * "/", if any).  If path is empty or the root directory ("/"), set
 * path to the empty string.
 */
extern void trim_last_path_elm(struct strbuf *path);

/*
 * path contains a path that might be a symlink.
 *
 * If path is a symlink, attempt to overwrite it with a path to the
 * real file or directory (which may or may not exist), following a
 * chain of symlinks if necessary.  Otherwise, leave path unmodified.
 *
 * This is a best-effort routine.  If an error occurs, path will
 * either be left unmodified or will name a different symlink in a
 * symlink chain that started with the original path.
 */
extern void resolve_symlink(struct strbuf *path);

#endif /* TEMPFILE_H */
