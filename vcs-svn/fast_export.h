#ifndef FAST_EXPORT_H_
#define FAST_EXPORT_H_

<<<<<<< HEAD
<<<<<<< HEAD
#include "line_buffer.h"

=======
=======
>>>>>>> vcs-svn: eliminate repo_tree structure
<<<<<<< HEAD
=======
#include "line_buffer.h"
=======
struct strbuf;
struct line_buffer;
>>>>>>> 7e69325... vcs-svn: eliminate repo_tree structure

void fast_export_init(int fd);
void fast_export_deinit(void);
void fast_export_reset(void);

<<<<<<< HEAD
<<<<<<< HEAD
>>>>>>> 8ab8687... vcs-svn: explicitly close streams used for delta application at exit
>>>>>>> vcs-svn: explicitly close streams used for delta application at exit
void fast_export_delete(uint32_t depth, uint32_t *path);
void fast_export_modify(uint32_t depth, uint32_t *path, uint32_t mode,
			uint32_t mark);
void fast_export_begin_commit(uint32_t revision);
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
=======
void fast_export_delete(uint32_t depth, const uint32_t *path);
void fast_export_modify(uint32_t depth, const uint32_t *path,
			uint32_t mode, const char *dataref);
=======
void fast_export_delete(const char *path);
void fast_export_modify(const char *path, uint32_t mode, const char *dataref);
>>>>>>> 01823f6... vcs-svn: pass paths through to fast-import
void fast_export_begin_commit(uint32_t revision, uint32_t author, char *log,
			uint32_t uuid, uint32_t url, unsigned long timestamp);
void fast_export_end_commit(uint32_t revision);
void fast_export_data(uint32_t mode, uint32_t len, struct line_buffer *input);
void fast_export_delta(uint32_t mode, const char *path,
			uint32_t old_mode, const char *dataref,
>>>>>>> 7e69325... vcs-svn: eliminate repo_tree structure
			uint32_t len, struct line_buffer *input);
>>>>>>> ae828d6... vcs-svn: do not rely on marks for old blobs
>>>>>>> vcs-svn: do not rely on marks for old blobs

void fast_export_ls_rev(uint32_t rev, const char *path,
			uint32_t *mode_out, struct strbuf *dataref_out);
void fast_export_ls(const char *path,
			uint32_t *mode_out, struct strbuf *dataref_out);

#endif
