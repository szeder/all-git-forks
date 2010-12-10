#ifndef FAST_EXPORT_H_
#define FAST_EXPORT_H_

<<<<<<< HEAD
#include "line_buffer.h"

=======
<<<<<<< HEAD
=======
#include "line_buffer.h"

void fast_export_init(int fd);
void fast_export_deinit(void);
void fast_export_reset(void);

>>>>>>> 8ab8687... vcs-svn: explicitly close streams used for delta application at exit
>>>>>>> vcs-svn: explicitly close streams used for delta application at exit
void fast_export_delete(uint32_t depth, uint32_t *path);
void fast_export_modify(uint32_t depth, uint32_t *path, uint32_t mode,
			uint32_t mark);
void fast_export_commit(uint32_t revision, uint32_t author, char *log,
			uint32_t uuid, uint32_t url, unsigned long timestamp);
<<<<<<< HEAD
void fast_export_blob(uint32_t mode, uint32_t mark, uint32_t len,
		      struct line_buffer *input);
=======
<<<<<<< HEAD
void fast_export_blob(uint32_t mode, uint32_t mark, uint32_t len);
=======
void fast_export_blob(uint32_t mode, uint32_t mark, uint32_t len,
		      struct line_buffer *input);
void fast_export_blob_delta(uint32_t mode, uint32_t mark,
			uint32_t old_mode, uint32_t old_mark,
			uint32_t len, struct line_buffer *input);
void fast_export_blob_delta_rev(uint32_t mode, uint32_t mark, uint32_t old_mode,
			uint32_t old_rev, const uint32_t *old_path,
			uint32_t len, struct line_buffer *input);
>>>>>>> ae828d6... vcs-svn: do not rely on marks for old blobs
>>>>>>> vcs-svn: do not rely on marks for old blobs

#endif
