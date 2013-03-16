#include "cache.h"
#include "run-command.h"
#include "vcs-cvs/client.h"
#include "vcs-cvs/proto-trace.h"
#include "pkt-line.h"
#include "sigchain.h"

#include <string.h>

#define DB_CACHE
#ifdef DB_CACHE
#include <db.h>
static DB *db_cache = NULL;
static DB *db_cache_branch = NULL;
static char *db_cache_branch_path = NULL;

static void db_cache_release(DB *db)
{
	if (db)
		db->close(db, 0);
}

static void db_cache_release_default()
{
	db_cache_release(db_cache);
	fprintf(stderr, "db_cache released\n");
	if (db_cache_branch) {
		db_cache_release(db_cache_branch);
		db_cache_branch = NULL;
		unlink(db_cache_branch_path);
		fprintf(stderr, "db_cache %s removed\n", db_cache_branch_path);
		free(db_cache_branch_path);
	}
}

static void db_cache_release_default_on_signal(int signo)
{
	db_cache_release_default();
	sigchain_pop(signo);
	raise(signo);
}

static DB *db_cache_init(const char *name, int *exists)
{
	DB *db;
	const char *db_dir;
	struct strbuf db_path = STRBUF_INIT;

	db_dir = getenv("GIT_CACHE_CVS_DIR");
	if (!db_dir)
		return NULL;

	if (db_create(&db, NULL, 0))
		die("cannot create db_cache descriptor");

	strbuf_addf(&db_path, "%s/%s", db_dir, name);

	if (exists) {
		*exists = 0;
		if (!access(db_path.buf, R_OK | W_OK))
			*exists = 1;
	}

	//db->set_bt_compress(db, NULL, NULL);

	if (db->open(db, NULL, db_path.buf, NULL, DB_BTREE, DB_CREATE, 0664) != 0)
		die("cannot open/create db_cache at %s", db_path.buf);

	strbuf_release(&db_path);
	return db;
}

static void db_cache_init_default()
{
	if (db_cache)
		return;

	db_cache = db_cache_init("cvscache.db", NULL);
	atexit(db_cache_release_default);
	sigchain_push_common(db_cache_release_default_on_signal);
}

static DBT *dbt_set(DBT *dbt, void *buf, size_t size)
{
	memset(dbt, 0, sizeof(*dbt));
	dbt->data = buf;
	dbt->size = size;

	return dbt;
}

static void db_cache_add(DB *db, const char *path, const char *revision, int isexec, struct strbuf *file)
{
	struct strbuf key_sb = STRBUF_INIT;
	int rc;
	DBT key;
	DBT value;

	if (!db)
		return;

	/*
	 * last byte of struct strbuf is used to store isexec bit.
	 */
	file->buf[file->len] = isexec;

	strbuf_addf(&key_sb, "%s:%s", revision, path);
	dbt_set(&key, key_sb.buf, key_sb.len);
	dbt_set(&value, file->buf, file->len+1); // +1 for isexec bit
	rc = db->put(db, NULL, &key, &value, DB_NOOVERWRITE);
	if (rc)
		error("db_cache put failed with rc: %d", rc);

	file->buf[file->len] = 0;
	strbuf_release(&key_sb);
}

static int db_cache_get(DB *db, const char *path, const char *revision, int *isexec, struct strbuf *file)
{
	struct strbuf key_sb = STRBUF_INIT;
	int rc;
	DBT key;
	DBT value;

	if (!db)
		return DB_NOTFOUND;

	strbuf_addf(&key_sb, "%s:%s", revision, path);
	dbt_set(&key, key_sb.buf, key_sb.len);
	memset(&value, 0, sizeof(value));
	rc = db->get(db, NULL, &key, &value, 0);
	if (rc && rc != DB_NOTFOUND)
		error("db_cache get failed with rc: %d", rc);

	if (!rc) {
		strbuf_reset(file);
		strbuf_add(file, value.data, value.size);
		/*
		 * last byte of value is used to store isexec bit.
		 */
		file->len--;
		*isexec = file->buf[file->len];
		file->buf[file->len] = 0;
	}

	strbuf_release(&key_sb);
	return rc;
}

static DB *db_cache_init_branch(const char *branch, time_t date, int *exists)
{
	DB *db;
	struct strbuf db_name = STRBUF_INIT;

	strbuf_addf(&db_name, "cvscache.%s.%ld.db", branch, date);
	db = db_cache_init(db_name.buf, exists);
	if (db) {
		db_cache_branch = db;
		db_cache_branch_path = strbuf_detach(&db_name, NULL);
	}
	else {
		strbuf_release(&db_name);
	}
	return db;
}

static void db_cache_release_branch(DB *db)
{
	db_cache_release(db);
	db_cache_branch = NULL;
	if (db_cache_branch_path) {
		free(db_cache_branch_path);
		db_cache_branch_path = NULL;
	}
}

static unsigned int hash_buf(const char *buf, size_t size)
{
	unsigned int hash = 0x12375903;

	while (size) {
		unsigned char c = *buf++;
		hash = hash*101 + c;
		size--;
	}
	return hash;
}

static int db_cache_for_each(DB *db, handle_file_fn_t cb, void *data)
{
	DBC *cur;
	DBT key;
	DBT value;
	int rc;
	void *p;
	struct cvsfile file = CVSFILE_INIT;

	if (db->cursor(db, NULL, &cur, 0))
		return -1;

	memset(&key, 0, sizeof(key));
	memset(&value, 0, sizeof(value));
	while (!(rc = cur->get(cur, &key, &value, DB_NEXT))) {
		strbuf_reset(&file.path);
		strbuf_reset(&file.revision);
		file.isdead = 0;
		file.isbin = 0;
		file.ismem = 1;
		strbuf_reset(&file.file);

		p = memchr(key.data, ':', key.size);
		if (!p)
			die("invalid db_cache key format");

		strbuf_add(&file.revision, key.data, p - key.data);
		p++;
		strbuf_add(&file.path, p, key.size - (p - key.data));
		strbuf_add(&file.file, value.data, value.size);
		/*
		 * last byte of value is used to store isexec bit.
		 */
		file.file.len--;
		file.isexec = file.file.buf[file.file.len];
		file.file.buf[file.file.len] = 0;

		fprintf(stderr, "db_cache foreach file: %s rev: %s size: %zu isexec: %u hash: %u\n",
			file.path.buf, file.revision.buf, file.file.len, file.isexec, hash_buf(file.file.buf, file.file.len));
		cb(&file, data);
	}

	cvsfile_release(&file);
	cur->close(cur);
	if (rc == DB_NOTFOUND)
		return 0;
	return -1;
}
#endif

static const char trace_key[] = "GIT_TRACE_CVS_PROTO";
static const char trace_proto[] = "CVS";
extern unsigned long fileMemoryLimit;

/*
 * [:method:][[user][:password]@]hostname[:[port]]/path/to/repository
 */

static void strbuf_copybuf(struct strbuf *sb, const char *buf, size_t len)
{
	sb->len = 0;

	strbuf_grow(sb, len + 1);
	strncpy(sb->buf, buf, len);
	sb->len = len;
	sb->buf[len] = '\0';
}

static inline void strbuf_copy(struct strbuf *sb, struct strbuf *sb2)
{
	strbuf_copybuf(sb, sb2->buf, sb2->len);
}

static inline void strbuf_copystr(struct strbuf *sb, const char *str)
{
	strbuf_copybuf(sb, str, strlen(str));
}

static int parse_cvsroot(struct cvs_transport *cvs, const char *cvsroot)
{
	const char *idx = 0;
	const char *next_tok = cvsroot;
	const char *colon;

	/*
	 * parse connect method
	 */
	if (cvsroot[0] == ':') {
		next_tok = cvsroot + 1;
		idx = strchr(next_tok, ':');
		if (!idx)
			return -1;

		if (!strncmp(next_tok, "ext", 3))
			cvs->protocol = cvs_proto_ext;
		else if (!strncmp(next_tok, "local", 5))
			cvs->protocol = cvs_proto_local;
		else if (!strncmp(next_tok, "pserver", 7))
			cvs->protocol = cvs_proto_pserver;

		next_tok = idx + 1;
	}
	else if (cvsroot[0] != '/')
		cvs->protocol = cvs_proto_ext;

	/*
	 * parse user/password
	 */
	idx = strchr(next_tok, '@');
	if (idx) {
		colon = strchr(next_tok, ':');
		if (colon < idx) {
			cvs->username = xstrndup(next_tok, colon - next_tok);
			next_tok = colon + 1;
			cvs->password = xstrndup(next_tok, idx - next_tok);
		}
		else {
			cvs->username = xstrndup(next_tok, idx - next_tok);
		}

		next_tok = idx + 1;
	}

	/*
	 * parse host/port
	 * FIXME: ipv6 support i.e. [fe80::c685:123:1234:1234%eth0]
	 */
	idx = strchr(next_tok, ']');
	if (!idx) {
		idx = strchr(next_tok, ':');
		if (!idx) {
			idx = strchr(next_tok, '/');
			if (!idx)
				return -1;
		}
	}
	else {
		++idx;
	}

	cvs->host = xstrndup(next_tok, idx - next_tok);

	if (*idx == '\0')
		return -1;

	/*
	 * FIXME: check dos drive prefix?
	 */
	if (*idx == ':') {
		char *port;
		next_tok = idx + 1;

		idx = strchr(next_tok, '/');
		if (!idx)
			return -1;

		port = xstrndup(next_tok, idx - next_tok);
		cvs->port = atoi(port);
		free(port);

		if (!cvs->port)
			return -1;

		next_tok = idx;
	}

	/*
	 * the rest is server repo path
	 */
	cvs->repo_path = xstrdup(next_tok);
	if (cvs->repo_path[strlen(cvs->repo_path)] == '/')
		cvs->repo_path[strlen(cvs->repo_path)] = '\0';

	if (!cvs->port) {
		switch (cvs->protocol) {
		case cvs_proto_ext:
			cvs->port = 22;
			break;
		case cvs_proto_pserver:
			cvs->port = 2401;
			break;
		default:
			break;
		}
	}

	return 0;
}

static struct child_process no_fork;

struct child_process *cvs_init_transport(struct cvs_transport *cvs,
				  const char *prog, int flags)
{
	struct child_process *conn = &no_fork;
	const char **arg;
	struct strbuf sport = STRBUF_INIT;
	struct strbuf userathost = STRBUF_INIT;

	/* Without this we cannot rely on waitpid() to tell
	 * what happened to our children.
	 */
	signal(SIGCHLD, SIG_DFL);

	if (cvs->protocol == cvs_proto_pserver) {
		die("cvs_proto_pserver unsupported");
		/*
		 * FIXME:
		 */
//		/* These underlying connection commands die() if they
//		 * cannot connect.
//		 */
//		char *target_host = xstrdup(host);
//		if (git_use_proxy(host))
//			conn = git_proxy_connect(fd, host);
//		else
//			git_tcp_connect(fd, host, flags);
//		/*
//		 * Separate original protocol components prog and path
//		 * from extended host header with a NUL byte.
//		 *
//		 * Note: Do not add any other headers here!  Doing so
//		 * will cause older git-daemon servers to crash.
//		 */
//		packet_write(fd[1],
//			     "%s %s%chost=%s%c",
//			     prog, path, 0,
//			     target_host, 0);
//		free(target_host);
//		free(url);
//		if (free_path)
//			free(path);
//		return conn;
	}

	conn = xcalloc(1, sizeof(*conn));
	conn->in = conn->out = -1;
	conn->argv = arg = xcalloc(7, sizeof(*arg));
	if (cvs->protocol == cvs_proto_ext) {
		const char *ssh = getenv("GIT_SSH");
		int putty = ssh && strcasestr(ssh, "plink");
		if (!ssh) ssh = "ssh";

		*arg++ = ssh;
		if (putty && !strcasestr(ssh, "tortoiseplink"))
			*arg++ = "-batch";
		if (cvs->port) {
			strbuf_addf(&sport, "%hu", cvs->port);

			/* P is for PuTTY, p is for OpenSSH */
			*arg++ = putty ? "-P" : "-p";
			*arg++ = sport.buf;
		}
		strbuf_addf(&userathost, "%s@%s", cvs->username, cvs->host);
		*arg++ = userathost.buf;
	}
	else {
		/* remove repo-local variables from the environment */
		//conn->env = local_repo_env;
		conn->use_shell = 1;
	}
	*arg++ = prog;
	*arg = NULL;

	if (start_command(conn))
		die("unable to fork");

	cvs->fd[0] = conn->out; /* read from child's stdout */
	cvs->fd[1] = conn->in;  /* write to child's stdin */

	strbuf_release(&sport);
	strbuf_release(&userathost);
	return conn;
}

static ssize_t z_write_in_full(int fd, git_zstream *wr_stream, const void *buf, size_t len)
{
	unsigned char zbuf[ZBUF_SIZE];
	unsigned long zlen;
	ssize_t written = 0;
	ssize_t ret;
	int flush = Z_NO_FLUSH;

	wr_stream->next_in = (void *)buf;
	wr_stream->avail_in = len;
	wr_stream->avail_out = sizeof(zbuf);

	while (wr_stream->avail_in ||
	//       !wr_stream->avail_out ||
	       flush == Z_NO_FLUSH) {

		wr_stream->next_out = zbuf;
		wr_stream->avail_out = sizeof(zbuf);

		if (git_deflate_bound(wr_stream, wr_stream->avail_in) <= sizeof(zbuf))
			flush = Z_SYNC_FLUSH;

		if (git_deflate(wr_stream, flush) != Z_OK)
			die("deflate failed");

		zlen = sizeof(zbuf) - wr_stream->avail_out;
		ret = write_in_full(fd, zbuf, zlen);
		if (ret == -1)
			return -1;
		written += ret;
	}

	proto_ztrace(len, written, OUT);
	if (flush != Z_SYNC_FLUSH)
		die("no Z_SYNC_FLUSH");
	return written;
}

enum {
	WR_NOFLUSH,
	WR_FLUSH
};

static ssize_t cvs_write(struct cvs_transport *cvs, int flush, const char *fmt, ...) __attribute__((format (printf, 3, 4)));
static ssize_t cvs_write(struct cvs_transport *cvs, int flush, const char *fmt, ...)
{
	va_list args;
	ssize_t written;

	if (fmt) {
		va_start(args, fmt);
		strbuf_vaddf(&cvs->wr_buf, fmt, args);
		va_end(args);
	}

	if (flush == WR_NOFLUSH)
		return 0;

	if (cvs->compress)
		written = z_write_in_full(cvs->fd[1], &cvs->wr_stream, cvs->wr_buf.buf, cvs->wr_buf.len);
	else
		written = write_in_full(cvs->fd[1], cvs->wr_buf.buf, cvs->wr_buf.len);

	if (written == -1)
		return -1;

	proto_trace(cvs->wr_buf.buf, cvs->compress ? cvs->wr_buf.len : written, OUT);
	strbuf_reset(&cvs->wr_buf);

	return 0;
}

/*static ssize_t cvs_write_full(struct cvs_transport *cvs, const char *buf, size_t len)
{
	ssize_t written;

	if (cvs->wr_buf.len)
		cvs_write(cvs, WR_FLUSH, NULL);

	if (cvs->compress)
		written = z_write_in_full(cvs->fd[1], &cvs->wr_stream, buf, len);
	else
		written = write_in_full(cvs->fd[1], buf, len);

	if (written == -1)
		return -1;

	cvs_proto_trace(cvs->wr_buf.buf, cvs->compress ? len : written, OUT_BLOB);
	strbuf_reset(&cvs->wr_buf);

	return 0;
}*/

static ssize_t z_xread(int fd, git_zstream *rd_stream, void *zbuf, size_t zbuf_len,
		       void *buf, size_t buf_len)
{
	ssize_t zreadn;
	ssize_t readn;
	int ret;

	if (!rd_stream->next_in) {
		rd_stream->next_in = zbuf;
		rd_stream->avail_in = 0;
	}

	rd_stream->next_out = buf;
	rd_stream->avail_out = buf_len;
	zreadn = rd_stream->avail_in;

	do {
		if (!rd_stream->avail_in) {
			zreadn = xread(fd, zbuf, zbuf_len);
			if (zreadn <= 0)
				return zreadn;
			rd_stream->next_in = zbuf;
			rd_stream->avail_in = zreadn;
		}

		ret = git_inflate(rd_stream, 0);
		if (ret != Z_OK && ret != Z_STREAM_END)
			die("inflate failed");

		zreadn -= rd_stream->avail_in;
		readn = buf_len - rd_stream->avail_out;
		proto_ztrace(readn, zreadn, IN);
	} while(!readn || ret);

	return readn;
}

static ssize_t z_read_in_full(int fd, git_zstream *rd_stream, void *zbuf, size_t zbuf_len,
		       void *buf, size_t count)
{
	char *p = buf;
	ssize_t total = 0;

	while (count > 0) {
		ssize_t loaded = z_xread(fd, rd_stream, zbuf, zbuf_len, p, count);
		if (loaded < 0)
			return -1;
		if (loaded == 0)
			return total;
		count -= loaded;
		p += loaded;
		total += loaded;
	}

	return total;
}

static ssize_t cvs_readline(struct cvs_transport *cvs, struct strbuf *sb)
{
	char *newline;
	ssize_t readn;
	size_t linelen;

	strbuf_reset(sb);

	while (1) {
		newline = memchr(cvs->rd_buf.buf, '\n', cvs->rd_buf.len);
		if (newline) {
			linelen = newline - cvs->rd_buf.buf;
			strbuf_add(sb, cvs->rd_buf.buf, linelen);

			if (trace_want(trace_key)) {
				sb->buf[sb->len] = '\n';
				proto_trace(sb->buf, sb->len + 1, IN);
				sb->buf[sb->len] = '\0';
			}

			cvs->rd_buf.buf += linelen + 1;
			cvs->rd_buf.len -= linelen + 1;
			return sb->len;
		}

		if (cvs->rd_buf.len) {
			strbuf_add(sb, cvs->rd_buf.buf, cvs->rd_buf.len);
			cvs->rd_buf.len = 0;
		}
		cvs->rd_buf.buf = cvs->rd_buf.data;

		if (cvs->compress)
			readn = z_xread(cvs->fd[0], &cvs->rd_stream,
					cvs->rd_zbuf, sizeof(cvs->rd_zbuf),
					cvs->rd_buf.buf, sizeof(cvs->rd_buf.data));
		else
			readn = xread(cvs->fd[0], cvs->rd_buf.buf, sizeof(cvs->rd_buf.data));

		if (readn <= 0) {
			proto_trace(NULL, 0, IN);
			return -1;
		}

		cvs->rd_buf.len = readn;
	}

	return -1;
}

static ssize_t _cvs_read_full_from_buf(struct cvs_transport *cvs, char *buf, ssize_t size)
{
	size_t readn;

	readn = cvs->rd_buf.len < size ? cvs->rd_buf.len : size;

	memcpy(buf, cvs->rd_buf.buf, readn);

	cvs->rd_buf.buf += readn;
	cvs->rd_buf.len -= readn;
	if (!cvs->rd_buf.len) {
		cvs->rd_buf.buf = cvs->rd_buf.data;
		cvs->rd_buf.len = 0;
	}

	return readn;
}

static ssize_t cvs_read_full(struct cvs_transport *cvs, char *buf, ssize_t size)
{
	ssize_t readn = 0;
	ssize_t ret;
	char *pbuf = buf;

	if (cvs->rd_buf.len) {
		readn = _cvs_read_full_from_buf(cvs, buf, size);

		pbuf += readn;
		size -= readn;
	}

	if (cvs->compress)
		ret = z_read_in_full(cvs->fd[0], &cvs->rd_stream,
				cvs->rd_zbuf, sizeof(cvs->rd_zbuf),
				pbuf, size);
	else
		ret = read_in_full(cvs->fd[0], pbuf, size);

	if (ret == -1)
		die("read full failed");

	readn += ret;
	proto_trace(buf, readn, IN_BLOB);
	return readn;
}

static ssize_t z_finish_write(int fd, git_zstream *wr_stream)
{
	unsigned char zbuf[ZBUF_SIZE];
	unsigned long zlen;
	ssize_t written = 0;

	wr_stream->next_in = (void *)NULL;
	wr_stream->avail_in = 0;

	wr_stream->next_out = zbuf;
	wr_stream->avail_out = sizeof(zbuf);

	if (git_deflate(wr_stream, Z_FINISH) != Z_STREAM_END)
		die("deflate finish failed");

	zlen = sizeof(zbuf) - wr_stream->avail_out;
	written = write_in_full(fd, zbuf, zlen);
	if (written == -1)
		return -1;

	proto_ztrace(0, written, OUT);
	return written;
}

static void z_terminate_gently(struct cvs_transport *cvs)
{
	ssize_t readn;

	z_finish_write(cvs->fd[1], &cvs->wr_stream);
	do {
		readn = z_xread(cvs->fd[0], &cvs->rd_stream,
				cvs->rd_zbuf, sizeof(cvs->rd_zbuf),
				cvs->rd_buf.buf, sizeof(cvs->rd_buf.data));
		proto_trace(cvs->rd_buf.buf, readn, IN);
	} while(readn);
}

static int strbuf_gettext_after(struct strbuf *sb, const char *what, struct strbuf *out)
{
	size_t len = strlen(what);
	if (!strncmp(sb->buf, what, len)) {
		strbuf_copybuf(out, sb->buf + len, sb->len - len);
		return 1;
	}
	return 0;
}

static int strbuf_startswith(struct strbuf *sb, const char *what)
{
	return !strncmp(sb->buf, what, strlen(what));
}

static int strbuf_endswith(struct strbuf *sb, const char *what)
{
	size_t len = strlen(what);
	if (sb->len < len)
		return 0;

	return !strcmp(sb->buf + sb->len - len, what);
}

static int cvs_getreply(struct cvs_transport *cvs, struct strbuf *sb, const char *reply)
{
	int found = 0;
	ssize_t ret;

	while (1) {
		ret = cvs_readline(cvs, &cvs->rd_line_buf);
		if (ret <= 0)
			return -1;

		if (strbuf_startswith(sb, "E "))
			fprintf(stderr, "CVS E: %s\n", sb->buf + 2);

		if (strbuf_gettext_after(&cvs->rd_line_buf, reply, sb))
			found = 1;

		if (!strcmp(cvs->rd_line_buf.buf, "ok"))
			break;

		if (strbuf_startswith(&cvs->rd_line_buf, "error")) {
			fprintf(stderr, "CVS Error: %s", cvs->rd_line_buf.buf);
			return -1;
		}
	}

	return !found;
}

static int cvs_getreply_firstmatch(struct cvs_transport *cvs, struct strbuf *sb, const char *reply)
{
	ssize_t ret;

	while (1) {
		ret = cvs_readline(cvs, &cvs->rd_line_buf);
		if (ret <= 0)
			return -1;

		if (strbuf_startswith(&cvs->rd_line_buf, "E "))
			fprintf(stderr, "CVS E: %s\n", cvs->rd_line_buf.buf + 2);

		if (strbuf_gettext_after(&cvs->rd_line_buf, reply, sb))
			return 0;

		if (!strcmp(cvs->rd_line_buf.buf, "ok"))
			return 1;

		if (strbuf_startswith(&cvs->rd_line_buf, "error")) {
			fprintf(stderr, "CVS Error: %s", cvs->rd_line_buf.buf);
			return -1;
		}
	}

	return -1;
}

static int cvs_init_compress(struct cvs_transport *cvs, int compress)
{
	if (!compress)
		return 0;

	if (cvs_write(cvs, WR_FLUSH, "Gzip-stream %d\n", compress) == -1)
		die("cvs_write failed");
	cvs->compress = compress;

	git_deflate_init(&cvs->wr_stream, compress);
	git_inflate_init(&cvs->rd_stream);

	return 0;
}

static int cvs_negotiate(struct cvs_transport *cvs)
{
	struct strbuf reply = STRBUF_INIT;
	ssize_t ret;

	ret = cvs_write(cvs,
			WR_FLUSH,
			"Root %s\n"
			//"Valid-responses ok error Valid-requests Checked-in New-entry Checksum Copy-file Updated Created Update-existing Merged Patched Rcs-diff Mode Mod-time Removed Remove-entry Set-static-directory Clear-static-directory Set-sticky Clear-sticky Template Notified Module-expansion Wrapper-rcsOption M Mbinary E F MT\n"
			"Valid-responses ok error Valid-requests Checked-in New-entry Checksum Copy-file Updated Created Merged Patched Rcs-diff Mode Mod-time Removed Remove-entry Set-static-directory Clear-static-directory Set-sticky Clear-sticky Template Notified Module-expansion Wrapper-rcsOption M E\n"
			"valid-requests\n",
			cvs->repo_path);

	if (ret == -1)
		die("failed to connect");

	ret = cvs_getreply(cvs, &reply, "Valid-requests ");
	if (ret)
		return -1;

	fprintf(stderr, "CVS Valid-responses: %s\n", reply.buf);

	ret = cvs_write(cvs, WR_NOFLUSH, "UseUnchanged\n");

	const char *gzip = getenv("GZIP");
	if (gzip)
		cvs_init_compress(cvs, atoi(gzip));
	else
		cvs_init_compress(cvs, 1);

	ret = cvs_write(cvs, WR_FLUSH, "version\n");
	ret = cvs_getreply(cvs, &reply, "M ");
	if (ret)
		return -1;

	fprintf(stderr, "CVS Server version: %s\n", reply.buf);
	strbuf_release(&reply);
	return 0;
}

static void strbuf_complete_line_ch(struct strbuf *sb, char ch)
{
	if (sb->len && sb->buf[sb->len - 1] != ch)
		strbuf_addch(sb, ch);
}

static void strbuf_rtrim_ch(struct strbuf *sb, char ch)
{
	if (sb->len > 0 && sb->buf[sb->len - 1] == ch)
		sb->len--;
	sb->buf[sb->len] = '\0';
}

struct cvs_transport *cvs_connect(const char *cvsroot, const char *module)
{
	struct cvs_transport *cvs;
	struct strbuf sb = STRBUF_INIT;

#ifdef DB_CACHE
	db_cache_init_default();
#endif

	cvs = xcalloc(1, sizeof(*cvs));
	cvs->rd_buf.buf = cvs->rd_buf.data;
	strbuf_init(&cvs->rd_line_buf, 0);
	strbuf_init(&cvs->wr_buf, 0);
	if (parse_cvsroot(cvs, cvsroot))
		die(_("Malformed cvs root format."));
	strbuf_copystr(&sb, module);
	strbuf_rtrim_ch(&sb, '/');
	cvs->module = strbuf_detach(&sb, NULL);

	strbuf_copystr(&sb, cvs->repo_path);
	strbuf_complete_line_ch(&sb, '/');
	if (cvs->module[0] == '/')
		die("CVS module name should not start with '/'");

	strbuf_addstr(&sb, cvs->module);
	strbuf_complete_line_ch(&sb, '/');

	cvs->full_module_path = strbuf_detach(&sb, NULL);

	switch (cvs->protocol) {
	case cvs_proto_ext:
	case cvs_proto_local:
		cvs->conn = cvs_init_transport(cvs, "cvs server", CONNECT_VERBOSE);
		if (!cvs->conn) {
			free(cvs);
			return NULL;
		}
		break;
	default:
		die(_("Unsupported cvs connection type."));
	}

	if (cvs_negotiate(cvs)) {
		error("CVS protocol negotiation failed.");
		cvs_terminate(cvs);
		return NULL;
	}

	strbuf_release(&sb);
	return cvs;
}

int cvs_terminate(struct cvs_transport *cvs)
{
	struct child_process *conn = cvs->conn;

	if (cvs->compress) {
		z_terminate_gently(cvs);
		git_deflate_end_gently(&cvs->wr_stream);
		git_inflate_end(&cvs->rd_stream);
	}

	close(cvs->fd[1]);
	close(cvs->fd[0]);
	strbuf_release(&cvs->rd_line_buf);
	strbuf_release(&cvs->wr_buf);

	if (cvs->host)
		free(cvs->host);
	if (cvs->username)
		free(cvs->username);
	if (cvs->password)
		free(cvs->password);

	if (cvs->repo_path)
		free(cvs->repo_path);
	if (cvs->module)
		free(cvs->module);
	if (cvs->full_module_path)
		free(cvs->full_module_path);
	free(cvs);

	return finish_connect(conn);
}

char **cvs_gettags(struct cvs_transport *cvs)
{
	char **tags;
	tags = xcalloc(16, sizeof(*tags));

	return tags;
}

struct branch_rev {
	struct strbuf name;
	struct strbuf rev;
};

struct branch_rev_list {
	unsigned int size, nr;
	struct branch_rev *array;
};

#define for_each_branch_rev_list_item(item,list) \
	for (item = (list)->array; item < (list)->array + (list)->nr; ++item)

void branch_rev_list_init(struct branch_rev_list *list)
{
	list->size = 0;
	list->nr = 0;
	list->array = NULL;
}

static void rev_list_grow(struct branch_rev_list *list, unsigned int nr)
{
	if (nr > list->size) {
		unsigned int was = list->size;
		struct branch_rev *it;
		if (alloc_nr(list->size) < nr)
			list->size = nr;
		else
			list->size = alloc_nr(list->size);
		list->array = xrealloc(list->array, list->size * sizeof(*list->array));
		for (it = list->array + was;
		     it < list->array + list->size;
		     it++) {
			strbuf_init(&it->name, 0);
			strbuf_init(&it->rev, 0);
		}
	}
}

void branch_rev_list_push(struct branch_rev_list *list, struct strbuf *name, struct strbuf *rev)
{
	rev_list_grow(list, list->nr + 1);

	strbuf_swap(&list->array[list->nr].name, name);
	strbuf_swap(&list->array[list->nr].rev, rev);
	++list->nr;
}

int branch_rev_list_find(struct branch_rev_list *list, struct strbuf *rev, struct strbuf *name)
{
	struct branch_rev *item;

	for_each_branch_rev_list_item(item, list)
		if (!strbuf_cmp(&item->rev, rev)) {
			strbuf_copybuf(name, item->name.buf, item->name.len);
			return 1;
		}

	return 0;
}

void branch_rev_list_clear(struct branch_rev_list *list)
{
	list->nr = 0;
}

void branch_rev_list_release(struct branch_rev_list *list)
{
	struct branch_rev *item;

	if (!list->array)
		return;

	for_each_branch_rev_list_item(item, list) {
		strbuf_release(&item->name);
		strbuf_release(&item->rev);
	}

	free(list->array);
	branch_rev_list_init(list);
}

int strip_last_rev_num(struct strbuf *branch_rev)
{
	char *idx;
	int num;

	idx = strrchr(branch_rev->buf, '.');
	if (!idx)
		return -1;

	num = atoi(idx+1);
	strbuf_setlen(branch_rev, idx - branch_rev->buf);

	return num;
}

enum {
	sym_invalid = 0,
	sym_branch,
	//sym_vendor_branch,
	sym_tag
};

/*
 * branch revision cases:
 * x.y.z   - vendor branch if odd number of numbers (even number of dots)
 * x.y.0.z - branch, has magic branch number 0, normalized to x.y.z
 * x.y     - tag, no magic branch number
 */
int parse_branch_rev(struct strbuf *branch_rev)
{
	char *dot;
	char *last_dot = NULL;
	char *prev_dot = NULL;
	int prev_num;
	int dots = 0;

	dot = branch_rev->buf;

	while (1) {
		dot = strchr(dot, '.');
		if (!dot)
			break;

		dots++;
		prev_dot = last_dot;
		last_dot = dot;
		dot++;
	}

	/*while (*dot) {
		if (*dot == '.') {
			dots++;
			prev_dot = last_dot;
			last_dot = dot;
		}
		dot++;
	}*/

	if (prev_dot) {
		if (!(dots % 2))
			return sym_branch; //sym_vendor_branch;

		prev_num = atoi(prev_dot + 1);
		if (!prev_num) {
			strbuf_remove(branch_rev, prev_dot - branch_rev->buf, last_dot - prev_dot);
			return sym_branch;
		}
		return sym_tag;
	}

	if (!last_dot)
		die("Cannot parse branch revision");
	return last_dot ? sym_tag : sym_invalid;
}

int parse_sym(struct strbuf *reply, struct strbuf *branch_name, struct strbuf *branch_rev)
{
	char *idx;

	idx = strchr(reply->buf, ':');
	if (!idx)
		return sym_invalid;

	strbuf_copybuf(branch_name, reply->buf, idx - reply->buf);

	++idx;
	if (isspace(*idx))
		++idx;

	strbuf_copystr(branch_rev, idx);

	return parse_branch_rev(branch_rev);
}

//int parse_sym(struct strbuf *reply, struct strbuf *branch_name, struct strbuf *branch_rev)
//{
//	char *idx;
//	int branch_num;
//	int rev_num;
//
//	idx = strchr(reply->buf, ':');
//	if (!idx)
//		return -1;
//
//	strbuf_add(branch_name, reply->buf, idx - reply->buf);
//
//	++idx;
//	if (isspace(*idx))
//		++idx;
//
//	strbuf_addstr(branch_rev, idx);
//
//	branch_num = strip_last_rev_num(branch_rev);
//	if (branch_rev <= 0)
//		die("Cannot parse CVS branch revision");
//
//	rev_num = strip_last_rev_num(branch_rev);
//	if (rev_num == -1) {
//		/*
//		 * FIXME: handle tags like: 1.5
//		 */
//		//error("Skipping CVS branch tag");
//		return -1;
//	}
//
//	if (rev_num == 0) {
//		strbuf_addf(branch_rev, ".%d", branch_num);
//		return 0;
//	}
//
//	/*
//	 * FIXME: handle vendor branches: 1.1.1
//	 * FIXME: handle tags like: 1.5.1.3
//	 */
//	//error("Skipping CVS branch tag or Vendor branch");
//	return -1;
//}

void trim_revision(struct strbuf *revision)
{
	char *p;

	p = revision->buf;
	while (*p && (isdigit(*p) || *p == '.'))
		p++;

	if (*p)
		strbuf_setlen(revision, p - revision->buf);
}

int is_revision_metadata(struct strbuf *reply)
{
	char *p1, *p2;

	if (!(p1 = strchr(reply->buf, ':')))
		return 0;

	p2 = strchr(reply->buf, ' ');

	if (p2 && p2 < p1)
		return 0;

	if (!strbuf_endswith(reply, ";"))
		return 0;

	return 1;
}

time_t date_to_unixtime(struct strbuf *date)
{
	struct tm date_tm;
	char *p;

	memset(&date_tm, 0, sizeof(date_tm));
	p = strptime(date->buf, "%Y/%m/%d %T", &date_tm);
	if (!p) {
		// try: 2013-01-18 13:28:28 +0000
		p = strptime(date->buf, "%Y-%m-%d %T", &date_tm);
		if (!p)
			return 0;
	}

	setenv("TZ", "UTC", 1);
	tzset();

	return mktime(&date_tm);
}

#define CVS_LOG_BOUNDARY "----------------------------"
#define CVS_FILE_BOUNDARY "============================================================================="

enum
{
	NEED_RCS_FILE		= 0,
	NEED_WORKING_FILE	= 1,
	NEED_SYMS		= 2,
	NEED_EOS		= 3,
	NEED_START_LOG		= 4,
	NEED_REVISION		= 5,
	NEED_DATE_AUTHOR_STATE	= 6,
	NEED_EOM		= 7,
	SKIP_LINES		= 8
};

int parse_cvs_rlog(struct cvs_transport *cvs, add_rev_fn_t cb, void *data)
{
	struct strbuf reply = STRBUF_INIT;
	ssize_t ret;

	struct strbuf file = STRBUF_INIT;
	struct strbuf revision = STRBUF_INIT;
	struct strbuf branch = STRBUF_INIT;
	struct strbuf author = STRBUF_INIT;
	struct strbuf message = STRBUF_INIT;
	time_t timestamp;
	int is_dead;

	int branches_max = 0;
	int branches;
	int tags_max = 0;
	int tags;
	int files = 0;
	int revs = 0;

	int state = NEED_RCS_FILE;
	int have_log;
	int skip_unknown = 0;

	/*
	 * branch revision -> branch name, hash created per file
	 */
	//struct hash_table branch_rev_hash;
	//init_hash(&branch_rev_hash);
	struct strbuf branch_name = STRBUF_INIT;
	struct strbuf branch_rev = STRBUF_INIT;

	struct branch_rev_list branch_list;
	branch_rev_list_init(&branch_list);

	size_t len;
	int read_rlog = 0;
	int write_rlog = 0;
	FILE *rlog = NULL;
	const char *rlog_path = getenv("GIT_CACHE_CVS_RLOG");
	if (rlog_path) {
		if (!access(rlog_path, R_OK)) {
			read_rlog = 1;
			rlog = fopen(rlog_path, "r");
			if (!rlog)
				die("cannot open %s for reading", rlog_path);
		}
		else {
			write_rlog = 1;
			rlog = fopen(rlog_path, "w");
			if (!rlog)
				die("cannot open %s for writing", rlog_path);
		}
	}
	//free(cvs->full_module_path);
	//cvs->full_module_path = xstrdup("/cvs/se/cvs/all/se/");
	//cvs->full_module_path = xstrdup("/cvs/zfsp/cvs/");
	strbuf_grow(&reply, CVS_MAX_LINE);

	while (1) {
		if (read_rlog) {
			if (!fgets(reply.buf, reply.alloc, rlog))
				break;

			len = strlen(reply.buf);
			strbuf_setlen(&reply, len);
			if (len && reply.buf[len - 1] == '\n')
				strbuf_setlen(&reply, len - 1);
		}
		else {
			ret = cvs_getreply_firstmatch(cvs, &reply, "M ");
			if (ret == -1)
				return -1;
			else if (ret == 1) /* ok from server */
				break;

			if (write_rlog)
				fprintf(rlog, "%s\n", reply.buf);
		}

		switch(state) {
		case NEED_RCS_FILE:
			if (strbuf_gettext_after(&reply, "RCS file: ", &file)) {
				branch_rev_list_clear(&branch_list);
				files++;
				branches = 0;
				tags = 0;

				if (strbuf_startswith(&file, cvs->full_module_path)) {
					char *attic;

					if (!strbuf_endswith(&file, ",v"))
						die("RCS file name does not end with ,v");
					strbuf_setlen(&file, file.len - 2);

					attic = strstr(file.buf, "/Attic/");
					if (attic)
						strbuf_remove(&file, attic - file.buf, strlen("/Attic"));

					strbuf_remove(&file, 0, strlen(cvs->full_module_path));

					state = NEED_SYMS;
				}
				else {
					state = NEED_WORKING_FILE;
				}
			}
			break;
		case NEED_WORKING_FILE:
			if (strbuf_gettext_after(&reply, "Working file: ", &file)) {
				die("Working file: %s", file.buf);
				state = NEED_SYMS;
			}
			else {
				state = NEED_RCS_FILE;
			}
			break;
		case NEED_SYMS:
			if (strbuf_startswith(&reply, "symbolic names:"))
				state = NEED_EOS;
			break;
		case NEED_EOS:
			if (!isspace(reply.buf[0])) {
				/* see cvsps_types.h for commentary on have_branches */
				//file->have_branches = 1;
				state = NEED_START_LOG;
			}
			else {
				strbuf_ltrim(&reply);

				switch (parse_sym(&reply, &branch_name, &branch_rev)) {
				case sym_branch:
					branches++;
					branch_rev_list_push(&branch_list, &branch_name, &branch_rev);
					break;
				case sym_tag:
					tags++;
					break;
				}
			}
			break;
		case NEED_START_LOG:
			if (!strcmp(reply.buf, CVS_LOG_BOUNDARY))
				state = NEED_REVISION;
			break;
		case NEED_REVISION:
			if (strbuf_gettext_after(&reply, "revision ", &revision)) {
				int num;

				revs++;

				strbuf_reset(&branch);
				strbuf_reset(&author);
				strbuf_reset(&message);
				timestamp = 0;
				is_dead = 0;
				/* The "revision" log line can include extra information 
				 * including who is locking the file --- strip that out.
				 */
				trim_revision(&revision);

				strbuf_add(&branch, revision.buf, revision.len);
				num = strip_last_rev_num(&branch);
				if (num == -1)
					die("Cannot parse revision: %s", revision.buf);

				if (!strchr(branch.buf, '.')) {
					strbuf_copystr(&branch, "HEAD");
				}
				else {
					if (!branch_rev_list_find(&branch_list, &branch, &branch)) {
						error("Cannot find branch for: %s rev: %s branch: %s",
						      file.buf, revision.buf, branch.buf);
						strbuf_copystr(&branch, "UNKNOWN");
						skip_unknown = 1;
					}
				}

				state = NEED_DATE_AUTHOR_STATE;

				/*
				 * FIXME: cvsps extra case in which state = NEED_EOM;
				 */
			}
			break;
		case NEED_DATE_AUTHOR_STATE:
			if (strbuf_startswith(&reply, "date: ")) {
				struct strbuf **tokens, **it;

				tokens = strbuf_split_max(&reply, ';', 4);
				it = tokens;

				strbuf_trim(*it);
				if (!strbuf_gettext_after(*it, "date: ", &reply))
					die("Cannot parse CVS rlog: date");
				strbuf_rtrim_ch(&reply, ';');

				timestamp = date_to_unixtime(&reply);
				if (!timestamp)
					die("Cannot parse CVS rlog date: %s", reply.buf);

				/*
				 * FIXME: is following data optional?
				 */
				it++;
				strbuf_trim(*it);
				if (!strbuf_gettext_after(*it, "author: ", &author))
					die("Cannot parse CVS rlog: author");
				strbuf_rtrim_ch(&author, ';');

				it++;
				strbuf_trim(*it);
				if (!strbuf_gettext_after(*it, "state: ", &reply))
					die("Cannot parse CVS rlog: state");
				strbuf_rtrim_ch(&reply, ';');

				if (!strcmp(reply.buf, "dead"))
					is_dead = 1;

				strbuf_list_free(tokens);

				state = NEED_EOM;
				have_log = 0;
			}
			break;
		case NEED_EOM:
			if (!strcmp(reply.buf, CVS_LOG_BOUNDARY))
				state = NEED_REVISION;
			else if (!strcmp(reply.buf, CVS_FILE_BOUNDARY))
				state = NEED_RCS_FILE;
			else if (have_log || !is_revision_metadata(&reply)) {
					have_log = 1;
					strbuf_add(&message, reply.buf, reply.len);
					strbuf_addch(&message, '\n');
			}

			if (state != NEED_EOM) {
				//fprintf(stderr, "BRANCH: %s\nREV: %s %s %s %d %lu\nMSG: %s--\n", branch.buf,
				//	file.buf, revision.buf, author.buf, is_dead, timestamp, message.buf);
				if (branches_max < branches)
					branches_max = branches;
				if (tags_max < tags)
					tags_max = tags;
				if (is_dead &&
				    !prefixcmp(message.buf, "file ") &&
				    strstr(message.buf, "was initially added on branch")) {
					fprintf(stderr, "skipping initial add to another branch file: %s rev: %s\n", file.buf, revision.buf);
				}
				else if (!skip_unknown)
					cb(branch.buf, file.buf, revision.buf, author.buf, message.buf, timestamp, is_dead, data);
				skip_unknown = 0;
			}
			break;
		}
	}

	if (rlog)
		fclose(rlog);

	if (state != NEED_RCS_FILE)
		die("Cannot parse rlog, parser state %d", state);

	fprintf(stderr, "REVS: %d FILES: %d BRANCHES: %d TAGS: %d\n", revs, files, branches_max, tags_max);

	strbuf_release(&branch_name);
	strbuf_release(&branch_rev);
	branch_rev_list_release(&branch_list);
	strbuf_release(&reply);
	strbuf_release(&file);
	strbuf_release(&revision);
	strbuf_release(&branch);
	strbuf_release(&author);
	strbuf_release(&message);
	return 0;
}

int cvs_rlog(struct cvs_transport *cvs, time_t since, time_t until, add_rev_fn_t cb, void *data)
{
	ssize_t ret;
	const char *rlog_path = getenv("GIT_CACHE_CVS_RLOG");
	if (rlog_path && !access(rlog_path, R_OK))
		return parse_cvs_rlog(cvs, cb, data);

	if (since) {
		ret = cvs_write(cvs,
				WR_NOFLUSH,
				"Argument -d\n"
				"Argument %s<1 Jan 2038 05:00:00 -0000\n",
				show_date(since, 0, DATE_RFC2822));
	}

	ret = cvs_write(cvs,
			WR_FLUSH,
			"Argument --\n"
			"Argument %s\n"
			"rlog\n",
			cvs->module);

	if (ret == -1)
		die("Cannot send rlog command");

	return parse_cvs_rlog(cvs, cb, data);
}

static int verify_revision(const char *revision, const char *entry)
{
	char *rev_start;
	char *rev_end;
	if (entry[0] != '/')
		return -1;

	rev_start = strchr(entry + 1, '/');
	if (!rev_start)
		return -1;

	rev_start++;
	rev_end = strchr(rev_start, '/');
	if (!rev_end)
		return -1;

	if (strncmp(revision, rev_start, rev_end - rev_start))
		return -1;

	return 0;
}

int parse_mode(const char *str)
{
	int mode = 0;
	int um = 0;
	int mm = 0;
	const char *p = str;
	while (*p) {
		switch (*p) {
		case ',':
			mode |= mm & um;
			mm = 0;
			um = 0;
			break;
		case 'u':
			um |= 0700;
			break;
		case 'g':
			um |= 0070;
			break;
		case 'o':
			um |= 0007;
			break;
		case 'r':
			mm |= 0444;
			break;
		case 'w':
			mm |= 0222;
			break;
		case 'x':
			mm |= 0111;
			break;
		case '=':
			break;
		default:
			return -1;
		}
		p++;
	}
	mode |= mm & mm;
	return mode;
}

static void cvsfile_reset(struct cvsfile *file)
{
	strbuf_reset(&file->path);
	strbuf_reset(&file->revision);
	file->isexec = 0;
	file->isdead = 0;
	file->isbin = 0;
	file->ismem = 0;
	file->iscached = 0;
	file->mode = 0;
	strbuf_reset(&file->file);
}

int cvs_checkout_rev(struct cvs_transport *cvs, const char *file, const char *revision, struct cvsfile *content)
{
	int rc = -1;
	ssize_t ret;

	cvsfile_reset(content);
	strbuf_copystr(&content->path, file);
	strbuf_copystr(&content->revision, revision);

#ifdef DB_CACHE
	int isexec = 0;
	if (!db_cache_get(db_cache, file, revision, &isexec, &content->file)) {
		content->isexec = isexec;
		content->ismem = 1;
		content->iscached = 1;
		//fprintf(stderr, "db_cache get file: %s rev: %s size: %zu isexec: %u hash: %u\n",
		//	file, revision, content->file.len, content->isexec, hash_buf(content->file.buf, content->file.len));
		return 0;
	}
#endif

	ret = cvs_write(cvs,
			WR_FLUSH,
			"Argument -N\n"
			"Argument -P\n"
			"Argument -kk\n"
			"Argument -r\n"
			"Argument %s\n"
			"Argument --\n"
			"Argument %s/%s\n"
			"Directory .\n"
			"%s\n"
			"co\n",
			revision,
			cvs->module, file,
			cvs->repo_path);

	if (ret == -1)
		die("rlog to connect");

	struct strbuf file_full_path = STRBUF_INIT;
	struct strbuf file_mod_path = STRBUF_INIT;
	int mode;
	size_t size;

	strbuf_addstr(&file_full_path, cvs->full_module_path);
	strbuf_complete_line_ch(&file_full_path, '/');
	strbuf_addstr(&file_full_path, file);

	strbuf_addstr(&file_mod_path, cvs->module);
	strbuf_complete_line_ch(&file_mod_path, '/');
	strbuf_addstr(&file_mod_path, file);

	while (1) {
		ret = cvs_readline(cvs, &cvs->rd_line_buf);
		if (ret <= 0)
			return -1;

		if (strbuf_startswith(&cvs->rd_line_buf, "E "))
			fprintf(stderr, "CVS E: %s\n", cvs->rd_line_buf.buf + 2);

		if (strbuf_startswith(&cvs->rd_line_buf, "Created") ||
		    strbuf_startswith(&cvs->rd_line_buf, "Updated")) {
			if (cvs_readline(cvs, &cvs->rd_line_buf) <= 0)
				return -1;
			if (strbuf_cmp(&cvs->rd_line_buf, &file_full_path) &&
			    strbuf_cmp(&cvs->rd_line_buf, &file_mod_path))
				die("Checked out file name doesn't match %s %s", cvs->rd_line_buf.buf, file_full_path.buf);

			if (cvs_readline(cvs, &cvs->rd_line_buf) <= 0)
				return -1;
			if (verify_revision(revision, cvs->rd_line_buf.buf))
				die("Checked out file revision doesn't match the one requested %s %s", revision, cvs->rd_line_buf.buf);

			if (cvs_readline(cvs, &cvs->rd_line_buf) <= 0)
				return -1;
			mode = parse_mode(cvs->rd_line_buf.buf);
			if (mode == -1)
				die("Cannot parse checked out file mode %s", cvs->rd_line_buf.buf);
			content->mode = mode;
			content->isexec = !!(mode & 0111);

			if (cvs_readline(cvs, &cvs->rd_line_buf) <= 0)
				return -1;
			size = atoi(cvs->rd_line_buf.buf);
			if (!size && strcmp(cvs->rd_line_buf.buf, "0"))
				die("Cannot parse file size %s", cvs->rd_line_buf.buf);

			//fprintf(stderr, "checkout %s rev %s mode %o size %zu\n", file, revision, mode, size);

			if (size) {
				// FIXME:
				if (size <= fileMemoryLimit) {
					content->ismem = 1;

					strbuf_grow(&content->file, size);
					ret = cvs_read_full(cvs, content->file.buf, size);
					if (ret == -1)
						die("Cannot checkout buf");
					if (ret < size)
						die("Cannot checkout buf: truncated: %zu read out of %zu", ret, size);

					strbuf_setlen(&content->file, size);

#ifdef DB_CACHE
					db_cache_add(db_cache, file, revision, content->isexec, &content->file);
					//content->iscached = 1;
					//fprintf(stderr, "db_cache add file: %s rev: %s size: %zu isexec: %u hash: %u\n",
					//	file, revision, content->file.len, content->isexec, hash_buf(content->file.buf, content->file.len));
#endif
				}
				else {
					// FIXME:
					die("Cannot checkout big file %s rev %s size %zu", file, revision, size);
				}
			}

			rc = 0;
		}

		if (strbuf_startswith(&cvs->rd_line_buf, "Removed") ||
		    strbuf_startswith(&cvs->rd_line_buf, "Remove-entry")) {
			if (cvs_readline(cvs, &cvs->rd_line_buf) <= 0)
				return -1;
			if (strbuf_cmp(&cvs->rd_line_buf, &file_full_path) &&
			    strbuf_cmp(&cvs->rd_line_buf, &file_mod_path))
				die("Checked out file name doesn't match %s %s", cvs->rd_line_buf.buf, file_full_path.buf);

			content->isdead = 1;
			rc = 0;
		}

		if (!strcmp(cvs->rd_line_buf.buf, "ok"))
			break;

		if (strbuf_startswith(&cvs->rd_line_buf, "error")) {
			fprintf(stderr, "CVS Error: %s", cvs->rd_line_buf.buf);
			break;
		}
	}

	strbuf_release(&file_full_path);
	strbuf_release(&file_mod_path);
	return rc;
}

static int parse_entry(const char *entry, struct strbuf *revision)
{
	char *rev_start;
	char *rev_end;
	if (entry[0] != '/')
		return -1;

	rev_start = strchr(entry + 1, '/');
	if (!rev_start)
		return -1;

	rev_start++;
	rev_end = strchr(rev_start, '/');
	if (!rev_end)
		return -1;

	strbuf_copybuf(revision, rev_start, rev_end - rev_start);
	return 0;
}

void cvsfile_release(struct cvsfile *file)
{
	strbuf_release(&file->path);
	strbuf_release(&file->revision);
	strbuf_release(&file->file);
}

int cvs_checkout_branch(struct cvs_transport *cvs, const char *branch, time_t date, handle_file_fn_t cb, void *data)
{
	int rc = -1;
	ssize_t ret;
#ifdef DB_CACHE
	int exists = 0;
	DB *db = NULL;
#endif
	if (date) {
#ifdef DB_CACHE
		db = db_cache_init_branch(branch, date, &exists);
		if (db && exists) {
			rc = db_cache_for_each(db, cb, data);
			db_cache_release_branch(db);
			return rc;
		}
#endif

		cvs_write(cvs,
			WR_NOFLUSH,
			"Argument -D\n"
			"Argument %s\n",
			show_date(date, 0, DATE_RFC2822));
	}
	ret = cvs_write(cvs,
			WR_FLUSH,
			"Argument -N\n"
			"Argument -P\n"
			"Argument -kk\n"
			"Argument -r\n"
			"Argument %s\n"
			"Argument --\n"
			"Argument %s\n"
			"Directory .\n"
			"%s\n"
			"co\n",
			branch ? branch : "HEAD",
			cvs->module,
			cvs->repo_path);

	if (ret == -1)
		die("rlog to connect");

	struct cvsfile file = CVSFILE_INIT;
	int mode;
	size_t size;

	while (1) {
		ret = cvs_readline(cvs, &cvs->rd_line_buf);
		if (ret <= 0)
			break;

		if (strbuf_startswith(&cvs->rd_line_buf, "E "))
			fprintf(stderr, "CVS E: %s\n", cvs->rd_line_buf.buf + 2);

		if (strbuf_startswith(&cvs->rd_line_buf, "Created") ||
		    strbuf_startswith(&cvs->rd_line_buf, "Updated")) {
			cvsfile_reset(&file);

			if (cvs_readline(cvs, &cvs->rd_line_buf) <= 0)
				break;
			if (!strbuf_gettext_after(&cvs->rd_line_buf, cvs->full_module_path, &file.path))
				die("Checked out file name doesn't start with module path %s %s", cvs->rd_line_buf.buf, cvs->full_module_path);

			if (cvs_readline(cvs, &cvs->rd_line_buf) <= 0)
				break;
			if (parse_entry(cvs->rd_line_buf.buf, &file.revision))
				die("Cannot parse checked out file entry line %s", cvs->rd_line_buf.buf);

			if (cvs_readline(cvs, &cvs->rd_line_buf) <= 0)
				break;
			mode = parse_mode(cvs->rd_line_buf.buf);
			if (mode == -1)
				die("Cannot parse checked out file mode %s", cvs->rd_line_buf.buf);
			file.mode = mode;
			file.isexec = !!(mode & 0111);

			if (cvs_readline(cvs, &cvs->rd_line_buf) <= 0)
				break;
			size = atoi(cvs->rd_line_buf.buf);
			if (!size && strcmp(cvs->rd_line_buf.buf, "0"))
				die("Cannot parse file size %s", cvs->rd_line_buf.buf);

			fprintf(stderr, "checkout %s rev %s mode %o size %zu\n", file.path.buf, file.revision.buf, mode, size);

			if (size) {
				// FIXME:
				if (size <= fileMemoryLimit) {
					file.ismem = 1;
					file.isdead = 0;

					strbuf_grow(&file.file, size);
					ret = cvs_read_full(cvs, file.file.buf, size);
					if (ret == -1)
						die("Cannot checkout buf");
					if (ret < size)
						die("Cannot checkout buf: truncated: %zu read out of %zu", ret, size);

					strbuf_setlen(&file.file, ret);
#ifdef DB_CACHE
					db_cache_add(db, file.path.buf, file.revision.buf, file.isexec, &file.file);
					file.iscached = 1;
					//fprintf(stderr, "db_cache branch add file: %s rev: %s size: %zu isexec: %u hash: %u\n",
					//	file.path.buf, file.revision.buf, file.file.len, file.isexec, hash_buf(file.file.buf, file.file.len));
#endif
				}
				else {
					// FIXME:
					die("Cannot checkout big file %s rev %s size %zu", file.file.buf, file.revision.buf, size);
				}
			}

			cb(&file, data);
		}

		if (strbuf_startswith(&cvs->rd_line_buf, "Removed") ||
		    strbuf_startswith(&cvs->rd_line_buf, "Remove-entry")) {
			cvsfile_reset(&file);

			if (cvs_readline(cvs, &cvs->rd_line_buf) <= 0)
				break;
			if (!strbuf_gettext_after(&cvs->rd_line_buf, cvs->full_module_path, &file.path))
				die("Checked out file name doesn't start with module path %s %s", cvs->rd_line_buf.buf, cvs->full_module_path);

			file.isdead = 1;
		}

		if (!strcmp(cvs->rd_line_buf.buf, "ok")) {
			rc = 0;
			break;
		}

		if (strbuf_startswith(&cvs->rd_line_buf, "error")) {
			fprintf(stderr, "CVS Error: %s", cvs->rd_line_buf.buf);
			break;
		}
	}

	cvsfile_release(&file);
#ifdef DB_CACHE
	db_cache_release_branch(db);
#endif
	return rc;
}

int cvs_status(struct cvs_transport *cvs, const char *file, const char *revision, int *status)
{
	ssize_t ret;

	struct strbuf basename = STRBUF_INIT;
	struct strbuf rel_dir = STRBUF_INIT;

	strbuf_copystr(&rel_dir, cvs->module);

	char *p = strrchr(file, '/');
	if (p) {
		strbuf_copystr(&basename, p+1);
		strbuf_addch(&rel_dir, '/');
		strbuf_add(&rel_dir, file, p - file);
	}
	else {
		strbuf_copystr(&basename, file);
	}

	ret = cvs_write(cvs,
			WR_FLUSH,
			"Argument --\n"
			"Directory %s\n"
			"%s/%s\n"
			"Entry /%s/%s///\n"
			"Unchanged %s\n"
			//"Is-modified %s\n"
			"Directory .\n"
			"%s\n"
			"status\n",
			rel_dir.buf,
			cvs->repo_path, rel_dir.buf,
			basename.buf, revision,
			basename.buf,
			cvs->repo_path);

	if (ret == -1)
		die("rlog to connect");

	ret = cvs_getreply(cvs, &basename, "ok");
	if (ret)
		return -1;

	return 0;
}

char *cvs_get_rev_branch(struct cvs_transport *cvs, const char *file, const char *revision)
{
	ssize_t ret;
	ret = cvs_write(cvs,
			WR_FLUSH,
			"Argument -h\n"
			"Argument --\n"
			"Argument %s/%s\n"
			"rlog\n",
			cvs->module, file);
	if (ret == -1)
		die("Cannot send rlog command");

	struct strbuf reply = STRBUF_INIT;
	struct strbuf branch = STRBUF_INIT;

	struct strbuf branch_name = STRBUF_INIT;
	struct strbuf branch_rev = STRBUF_INIT;

	size_t len;
	strbuf_grow(&reply, CVS_MAX_LINE);
	int state = NEED_RCS_FILE;
	int found = 0;

	strbuf_addstr(&branch, revision);
	strip_last_rev_num(&branch);

	while (1) {
		ret = cvs_getreply_firstmatch(cvs, &reply, "M ");
		if (ret == -1)
			return NULL;
		else if (ret == 1) /* ok from server */
			break;

		len = strlen(reply.buf);
		strbuf_setlen(&reply, len);
		if (len && reply.buf[len - 1] == '\n')
			strbuf_setlen(&reply, len - 1);

		switch(state) {
		case NEED_RCS_FILE:
			if (!prefixcmp(reply.buf, "RCS file: "))
				state = NEED_SYMS;
			break;
		case NEED_SYMS:
			if (!prefixcmp(reply.buf, "symbolic names:"))
				state = NEED_EOS;
			break;
		case NEED_EOS:
			if (!isspace(reply.buf[0])) {
				state = SKIP_LINES;
			}
			else {
				strbuf_ltrim(&reply);

				if (parse_sym(&reply, &branch_name, &branch_rev) == sym_branch &&
				    !strbuf_cmp(&branch_rev, &branch)) {
					found = 1;
					state = SKIP_LINES;
				}
			}
			break;
		default:
			break;
		}
	}

	if (state != SKIP_LINES)
		die("Cannot parse revision rlog, parser state %d", state);

	strbuf_release(&branch_rev);
	strbuf_release(&reply);
	strbuf_release(&branch);

	if (found)
		return strbuf_detach(&branch_name, NULL);

	strbuf_release(&branch_name);
	return NULL;
}
