#ifndef CLIENT_H
#define CLIENT_H

#include <time.h>

#define CVS_MAX_LINE 4096
#define ZBUF_SIZE 16384
#define RDBUF_SIZE ZBUF_SIZE

/*
 * TODO:
 * - add cvs sessions id or smth to trace
 * - add error messages string_list, do not perform die
 */

enum cvs_protocol {
	cvs_proto_local,
	cvs_proto_pserver,
	cvs_proto_ext
};

struct cvs_transport {
	int fd[2];
	struct child_process *conn;
	enum cvs_protocol protocol;
	char *host;
	char *username;
	char *password;

	char *repo_path;
	char *module;
	char *full_module_path;
	uint16_t port;

	struct strbuf  wr_buf;
	struct {
		size_t len;
		char *buf;
		char data[RDBUF_SIZE];
	} rd_buf;
	struct strbuf  rd_line_buf;

	int compress;
	git_zstream wr_stream;
	git_zstream rd_stream;
	char rd_zbuf[ZBUF_SIZE];
};

/*
 * rlog
 * branch name -> struct branch_meta (hash), create if none yet
 * per file: branch revision -> struct branch_meta (hash)
 *
 */

/*
 * connect to cvs server, complete protocol negotiation
 */
struct cvs_transport *cvs_connect(const char *cvsroot, const char *module);
int cvs_terminate(struct cvs_transport *cvs);

/*
 * generate metadata
 */
typedef void (*add_rev_fn_t)(const char *branch,
				  const char *path,
				  const char *revision,
				  const char *author,
				  const char *msg,
				  time_t timestamp,
				  int isdead,
				  void *data);

int cvs_rlog(struct cvs_transport *cvs, time_t since, time_t until, add_rev_fn_t cb, void *data);

/*
 * list of files
 */
struct cvsfile {
	struct strbuf path;
	struct strbuf revision;
	unsigned int isexec:1;
	unsigned int isdead:1;
	unsigned int isbin:1;        /* for commit */
	unsigned int ismem:1;        /* true if file contents are in file variable, false if just temp file name */
	unsigned int isnew:1;
	unsigned int iscached:1;
	unsigned int handled:1;
	unsigned int mode;
	struct strbuf file; /* FIXME: file or path depends on ismem */
	void *util;         /* used to store sha1 during check in (used in prepare file content callback) */
};

#define CVSFILE_INIT { STRBUF_INIT, STRBUF_INIT, 0, 0, 0, 0, 0, 0, 0, 0, STRBUF_INIT, NULL };
void cvsfile_init(struct cvsfile *file);
void cvsfile_release(struct cvsfile *file);

typedef void (*handle_file_fn_t)(struct cvsfile *file, void *data);
int cvs_checkout_branch(struct cvs_transport *cvs, const char *branch, time_t date, handle_file_fn_t cb, void *data);

/*
 * content should be preallocated
 */
int cvs_checkout_rev(struct cvs_transport *cvs, const char *file, const char *revision, struct cvsfile *content);

/*
 *
 */
int cvs_status(struct cvs_transport *cvs, const char *cvs_branch, struct cvsfile *files, int count);

int cvs_create_directories(struct cvs_transport *cvs, const char *cvs_branch, struct string_list *dirs);

typedef int (*prepare_file_content_fn_t)(struct cvsfile *file, void *data);
typedef void (*release_file_content_fn_t)(struct cvsfile *file, void *data);
int cvs_checkin(struct cvs_transport *cvs, const char *cvs_branch, const char *message,
		struct cvsfile *files, int count,
		prepare_file_content_fn_t prepare_cb,
		release_file_content_fn_t release_cb,
		void *data);

char *cvs_get_rev_branch(struct cvs_transport *cvs, const char *file, const char *revision);

#endif
