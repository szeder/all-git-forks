#include "remote-svn.h"
#include "version.h"
#include "quote.h"
#include <openssl/md5.h>

#ifndef NO_PTHREADS
#include <pthread.h>
#else
#define pthread_mutex_lock(x)
#define pthread_mutex_unlock(x)
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define malformed_die(c) die("protocol error %s:%d %s", __FILE__, __LINE__, (c)->indbg.buf)

struct conn {
	int fd, b, e;
	char in[4096];
	struct strbuf indbg, buf, word;
};

static const char *user_agent;
static struct strbuf url = STRBUF_INIT;
static struct credential *svn_auth;
struct conn main_connection;

static void init_connection(struct conn *c) {
	memset(c, 0, sizeof(*c));
	c->fd = -1;
	strbuf_init(&c->buf, 0);
	strbuf_init(&c->indbg, 0);
	strbuf_init(&c->word, 0);
}

static void reset_connection(struct conn *c) {
	close(c->fd);
	c->fd = -1;
	c->b = c->e = 0;
	strbuf_release(&c->buf);
	strbuf_release(&c->word);
	strbuf_release(&c->indbg);
}

static int readc(struct conn *c) {
	if (c->b == c->e) {
		c->b = 0;
		c->e = xread(c->fd, c->in, sizeof(c->in));
		if (c->e <= 0) return EOF;
	}

	return c->in[c->b++];
}

static void unreadc(struct conn *c) {
	c->b--;
}

static ssize_t read_svn(struct conn *c, void* p, size_t n) {
	/* big reads we may as well read directly into the target */
	if (c->e == c->b && n >= sizeof(c->in) / 2) {
		return xread(c->fd, p, n);

	} else if (c->e == c->b) {
		c->b = 0;
		c->e = xread(c->fd, c->in, sizeof(c->in));
		if (c->e <= 0) return c->e;
	}

	n = min(n, c->e - c->b);
	memcpy(p, c->in + c->b, n);
	c->b += n;
	return n;
}

static void writedebug(struct conn *c, struct strbuf *s, int tx) {
	struct strbuf buf = STRBUF_INIT;
	strbuf_addf(&buf, "S%c", tx ? '+' : '-');
	if (s->len && s->buf[0] != ' ')
		strbuf_addch(&buf, ' ');
	quote_c_style_counted(s->buf, s->len - 1, &buf, NULL, 1);
	strbuf_addch(&buf, '\n');
	fwrite(buf.buf, 1, buf.len, stderr);
	strbuf_release(&buf);
}

__attribute__((format (printf,2,3)))
static void sendf(struct conn *c, const char* fmt, ...);

static void sendf(struct conn *c, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	strbuf_reset(&c->buf);
	strbuf_vaddf(&c->buf, fmt, ap);

	if (svndbg >= 2)
		writedebug(c, &c->buf, 1);

	if (write_in_full(c->fd, c->buf.buf, c->buf.len) != c->buf.len)
		die_errno("write");
}

/* returns -1 if it can't find a number */
static ssize_t read_number(struct conn *c) {
	ssize_t v;

	for (;;) {
		int ch = readc(c);
		if ('0' <= ch && ch <= '9') {
			v = ch - '0';
			break;
		} else if (ch != ' ' && ch != '\n') {
			unreadc(c);
			return -1;
		}
	}

	for (;;) {
		int ch = readc(c);
		if (ch < '0' || ch > '9') {
			unreadc(c);
			if (svndbg >= 2)
				strbuf_addf(&c->indbg, " %d", (int) v);
			return v;
		}

		if (v > INT64_MAX/10) {
			die(_("number too big"));
		} else {
			v = 10*v + (ch - '0');
		}
	}
}

/* returns -1 if it can't find a list */
static int read_list(struct conn *c) {
	for (;;) {
		int ch = readc(c);
		if (ch == '(') {
			if (svndbg >= 2)
				strbuf_addstr(&c->indbg, " (");
			return 0;
		} else if (ch != ' ' && ch != '\n') {
			unreadc(c);
			return -1;
		}
	}
}

/* returns "" if it can't find an atom, string only valid until next
 * call to read_word */
static const char *read_word(struct conn *c) {
	int ch;

	strbuf_reset(&c->word);

	for (;;) {
		ch = readc(c);
		if (('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z')) {
			break;
		} else if (ch != ' ' && ch != '\n') {
			unreadc(c);
			return "";
		}
	}

	while (('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z')
			|| ('0' <= ch && ch <= '9')
			|| ch == '-')
	{
		strbuf_addch(&c->word, ch);
		ch = readc(c);
	}

	unreadc(c);
	if (svndbg >= 2)
		strbuf_addf(&c->indbg, " %s", c->word.buf);
	return c->word.buf;
}

/* returns -1 if no string or an invalid string */
static int append_string(struct conn *c, struct strbuf* s, int limitdbg) {
	size_t i;
	ssize_t n = read_number(c);
	if (n < 0 || unsigned_add_overflows(s->len, (size_t) n))
		return -1;
	if (readc(c) != ':')
		die(_("malformed string"));
	if (svndbg >= 2)
		strbuf_addch(&c->indbg, ':');

	strbuf_grow(s, s->len + n);

	i = 0;
	while (i < n) {
		ssize_t r = read_svn(c, s->buf + s->len, n-i);
		if (r < 0)
			die_errno("read error");
		if (r == 0)
			die("short read");
		strbuf_setlen(s, s->len + r);
		i += r;
	}

	if (svndbg >= 2) {
		if (limitdbg && n > 20) {
			strbuf_add(&c->indbg, s->buf + s->len - n, 20);
			strbuf_addstr(&c->indbg, "...");
		} else {
			strbuf_add(&c->indbg, s->buf + s->len - n, n);
		}
	}

	return 0;
}

static int read_string(struct conn *c, struct strbuf *s) {
	strbuf_reset(s);
	return append_string(c, s, 0);
}

static int do_skip_one(struct conn *c, int ch) {
	if (ch == EOF) {
		return -1;

	} else if ('0' <= ch && ch <= '9') {
		char buf[4096];
		ssize_t n, dbg = 0;

		n = read_number(c);
		if (n < 0)
			return -1;

		if (readc(c) != ':') {
			/* number */
			unreadc(c);
			return 0;
		}

		/* string */
		if (svndbg >= 2) {
			strbuf_addch(&c->indbg, ':');
			dbg = min(20, n);
		}

		while (n) {
			ssize_t r = read_svn(c, buf, min(n, sizeof(buf)));
			if (r <= 0)
				die_errno("read");

			if (dbg >= r) {
				strbuf_add(&c->indbg, buf, r);
			} else if (dbg > 0) {
				strbuf_add(&c->indbg, buf, dbg);
				strbuf_addstr(&c->indbg, "...");
			}

			dbg -= r;
			n -= r;
		}

		return 0;

	} else {
		if (*read_word(c) == '\0')
			return -1;

		return 0;
	}
}

static int read_end(struct conn *c) {
	int parens = 1;
	while (parens > 0) {
		int ch = readc(c);

		while (ch == ' ' || ch == '\n') {
			ch = readc(c);
		}

		if (ch == ')') {
			if (svndbg >= 2)
				strbuf_addstr(&c->indbg, " )");
			parens--;

		} else if (ch == '(') {
			if (svndbg >= 2)
				strbuf_addstr(&c->indbg, " (");
			parens++;

		} else {
			unreadc(c);
			if (do_skip_one(c, ch))
				return -1;
		}
	}
	return 0;
}

static int skip_next(struct conn *c) {
	int ch = readc(c);

	while (ch == ' ' || ch == '\n') {
		ch = readc(c);
	}

	if (ch == ')') {
		if (svndbg >= 2)
			strbuf_addstr(&c->indbg, " )");
		return -1;

	} else if (ch == '(') {
		if (svndbg >= 2)
			strbuf_addstr(&c->indbg, " (");
		return read_end(c);

	} else {
		unreadc(c);
		return do_skip_one(c, ch);
	}
}

static const char* read_command(struct conn *c) {
	const char *cmd;

	if (read_list(c)) return "";

	cmd = read_word(c);
	if (read_list(c)) return "";

	if (!strcmp(cmd, "failure")) {
		while (!read_list(c)) {
			struct strbuf msg = STRBUF_INIT;
			read_number(c);
			read_string(c, &msg);
			error("%s", msg.buf);
			strbuf_release(&msg);
			if (read_end(c))
				return "";
		}
	}

	return cmd;
}

static void read_newline(struct conn *c) {
	if (svndbg >= 2) {
		strbuf_addch(&c->indbg, '\n');
		writedebug(c, &c->indbg, 0);
		strbuf_reset(&c->indbg);
	}
}

static int read_command_end(struct conn *c) {
	if (read_end(c) || read_end(c))
		return -1;
	read_newline(c);
	return 0;
}

static int read_success(struct conn *c) {
	int ret = strcmp(read_command(c), "success");
	if (read_command_end(c))
		return -1;
	return ret;
}

static int read_done(struct conn *c) {
	if (strcmp(read_word(c), "done"))
		return -1;
	read_newline(c);
	return 0;
}

/* returns 0 if the list is missing or empty (and skips over it), 1 if
 * its present and has values */
static int have_optional(struct conn *c) {
	if (read_list(c))
		return 0;
	for (;;) {
		int ch = readc(c);
		if (ch == ')') {
			if (svndbg >= 2)
				strbuf_addstr(&c->indbg, " )");
			return 0;
		} else if (ch != ' ' && ch != '\n') {
			unreadc(c);
			return 1;
		}
	}
}

static void cram_md5(struct conn *c, const char* user, const char* pass) {
	unsigned char hash[16];
	struct strbuf chlg = STRBUF_INIT;
	HMAC_CTX hmac;

	if (strcmp(read_command(c), "step")) malformed_die(c);
	if (read_string(c, &chlg)) malformed_die(c);

	if (read_command_end(c)) malformed_die(c);

	HMAC_Init(&hmac, (unsigned char*) pass, strlen(pass), EVP_md5());
	HMAC_Update(&hmac, (unsigned char*) chlg.buf, chlg.len);
	HMAC_Final(&hmac, hash, NULL);
	HMAC_CTX_cleanup(&hmac);

	sendf(c, "%d:%s %s\n", (int) (strlen(user) + 1 + 32), user, md5_to_hex(hash));

	strbuf_release(&chlg);
}

static void svn_connect(struct conn *c, struct strbuf *uuid) {
	static char *host, *port;

	struct addrinfo hints, *res, *ai;
	int err;
	int fd = -1;
	int anon = 0, md5auth = 0;

	if (c->fd >= 0)
		return;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;

	if (!host) {
		host = xstrdup(svn_auth->host);
		port = strchr(host, ':');
		if (port) *(port++) = '\0';
	}

	err = getaddrinfo(host, port ? port : "3690", &hints, &res);

	if (err)
		die_errno("failed to connect to %s", url.buf);

	for (ai = res; ai != NULL; ai = ai->ai_next) {
		fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (fd < 0) continue;

		if (connect(fd, ai->ai_addr, ai->ai_addrlen)) {
			int err = errno;
			close(fd);
			errno = err;
			continue;
		}

		break;
	}

	if (fd < 0)
		die_errno("failed to connect to %s", url.buf);

	c->fd = fd;

	/* TODO: capabilities, mechs */
	sendf(c, "( 2 ( edit-pipeline svndiff1 ) %d:%s %d:%s ( ) )\n",
			(int) url.len, url.buf,
			(int) strlen(user_agent), user_agent);

	/* server hello: ( success ( minver maxver ) ) */
	if (strcmp(read_command(c), "success")) malformed_die(c);
	if (read_number(c) > 2 || read_number(c) < 2) malformed_die(c);
	if (read_command_end(c)) malformed_die(c);

	/* server mechs: ( success ( ( mech... ) realm ) ) */
	if (strcmp(read_command(c), "success")) malformed_die(c);
	if (read_list(c)) malformed_die(c);
	for (;;) {
		const char *mech = read_word(c);
		if (!strcmp(mech, "")) {
			break;
		} else if (!strcmp(mech, "ANONYMOUS")) {
			anon = 1;
		} else if (!strcmp(mech, "CRAM-MD5")) {
			md5auth = 1;
		}
	}
	if (read_end(c)) malformed_die(c);
	if (read_command_end(c)) malformed_die(c);

	if (!svn_auth->username && anon) {
		/* argument is "anonymous" encoded in base64 */
		sendf(c, "( ANONYMOUS ( 16:YW5vbnltb3VzCg== ) )\n");

		if (!read_success(c))
			goto auth_success;
	}

	if (md5auth) {
		sendf(c, "( CRAM-MD5 ( ) )\n");
		credential_fill(svn_auth);
		cram_md5(c, svn_auth->username, svn_auth->password);

		if (!read_success(c))
			goto auth_success;

		if (c == &main_connection)
			credential_reject(svn_auth);
	}

	die("auth failure");

auth_success:
	if (c == &main_connection)
		credential_approve(svn_auth);

	/* repo-info: ( success ( uuid repos-url ) ) */
	if (strcmp(read_command(c), "success")) malformed_die(c);
	if (uuid) {
		if (read_string(c, uuid)) malformed_die(c);
		if (read_string(c, &url)) malformed_die(c);
	}
	if (read_command_end(c)) malformed_die(c);

	/* reparent */
	sendf(c, "( reparent ( %d:%s ) )\n", (int) url.len, url.buf);
	if (read_success(c)) malformed_die(c);
	if (read_success(c)) malformed_die(c);
}

static void svn_change_user(struct credential *cred) {
	struct conn *c = &main_connection;
	struct credential *old = svn_auth;

	svn_auth = cred;

	if (c->fd >= 0 && old != cred) {
		reset_connection(c);
		svn_connect(c, NULL);
	}
}

static int svn_get_latest(void) {
	struct conn *c = &main_connection;
	int64_t n;
	sendf(c, "( get-latest-rev ( ) )\n");

	if (read_success(c)) malformed_die(c);
	if (strcmp(read_command(c), "success")) malformed_die(c);
	n = read_number(c);
	if (n < 0 || n > INT_MAX) malformed_die(c);
	if (read_command_end(c)) malformed_die(c);

	return (int) n;
}

static int svn_isdir(const char *path, int rev) {
	struct conn *c = &main_connection;
	const char *s = NULL;

	sendf(c, "( check-path ( %d:%s ( %d ) ) )\n",
		(int) strlen(path),
		path,
		rev);

	if (read_success(c)) return 0;

	if (!strcmp(read_command(c), "success")) {
		s = read_word(c);
	}
	if (read_command_end(c)) malformed_die(c);

	return !strcmp(s, "dir");
}

static void svn_list(const char *path, int rev, struct string_list *dirs) {
	struct conn *c = &main_connection;
	struct strbuf buf = STRBUF_INIT;

	sendf(c, "( get-dir ( %d:%s ( %d ) false true ( kind ) ) )\n",
		(int) strlen(path), path, rev);

	if (read_success(c)) return;

	if (!strcmp(read_command(c), "success")) {
		if (skip_next(c)) malformed_die(c); /* rev */
		if (skip_next(c)) malformed_die(c); /* props */
		if (read_list(c)) malformed_die(c); /* dirents */

		while (!read_list(c)) {
			if (read_string(c, &buf))
				malformed_die(c);

			if (!strcmp(read_word(c), "dir")) {
				clean_svn_path(&buf);
				string_list_insert(dirs, buf.buf);
			}

			if (read_end(c))
				malformed_die(c);
		}

		if (read_end(c)) malformed_die(c);
	}
	if (read_command_end(c)) malformed_die(c);

	strbuf_release(&buf);
}

static struct mergeinfo *svn_get_mergeinfo(const char *path, int rev) {
	struct conn *c = &main_connection;
	struct strbuf buf = STRBUF_INIT;
	struct mergeinfo *ret = NULL;

	sendf(c, "( get-dir ( %d:%s ( %d ) true false ) )\n",
		(int) strlen(path), path, rev);

	if (read_success(c)) return NULL;

	if (!strcmp(read_command(c), "success")) {
		if (skip_next(c)) malformed_die(c); /* rev */
		if (read_list(c)) malformed_die(c); /* props */

		while (!ret && !read_list(c)) {
			if (read_string(c, &buf)) malformed_die(c);

			if (!strcmp(buf.buf, "svn:mergeinfo")) {
				if (read_string(c, &buf)) malformed_die(c);
				ret = parse_svn_mergeinfo(buf.buf);
			}

			if (read_end(c)) malformed_die(c);
		}

		if (read_end(c)) malformed_die(c);
	}

	if (read_command_end(c)) malformed_die(c);

	strbuf_release(&buf);
	return ret;
}





static void svn_read_log(struct svnref **refs, int refnr, int start, int end) {
	struct conn *c = &main_connection;
	struct strbuf name = STRBUF_INIT;
	struct strbuf author = STRBUF_INIT;
	struct strbuf time = STRBUF_INIT;
	struct strbuf msg = STRBUF_INIT;
	struct strbuf paths = STRBUF_INIT;
	struct strbuf copy = STRBUF_INIT;
	int64_t rev;
	int i;

	for (i = 0; i < refnr; i++) {
		strbuf_addf(&paths, "%d:%s ", (int) strlen(refs[i]->path), refs[i]->path);
	}

	sendf(c, "( log ( ( %s) " /* (path...) */
		"( %d ) ( %d ) " /* start/end revno */
		"true true " /* changed-paths strict-node */
		") )\n",
		paths.buf,
		end,
		start
	     );

	if (read_success(c)) malformed_die(c);

	/* svn log reply is of the form
	 * ( ( ( n:changed-path A|D|R|M ( n:copy-path copy-rev ) ) ... ) rev n:author n:date n:message )
	 * ....
	 * done
	 * ( success ( ) )
	 */

	for (;;) {
		/* start of log entry */
		if (read_list(c)) {
			read_done(c);
			if (read_success(c)) malformed_die(c);
			break;
		}

		/* start changed path entries */
		if (read_list(c)) malformed_die(c);

		while (!read_list(c)) {
			/* path A|D|R|M [copy-path copy-rev] */
			int ismodify;
			int64_t copyrev = -1;
			if (read_string(c, &name)) malformed_die(c);
			ismodify = !strcmp(read_word(c), "M");

			if (have_optional(c)) {
				/* copy-path, copy-rev */
				if (read_string(c, &copy)) malformed_die(c);
				copyrev = read_number(c);
				if (copyrev <= 0 || copyrev > INT_MAX) malformed_die(c);
				if (read_end(c)) malformed_die(c);
				clean_svn_path(&copy);
			}

			clean_svn_path(&name);

			changed_path_read(refs, refnr, ismodify, name.buf, copy.buf, copyrev);

			if (read_end(c)) malformed_die(c);
		}

		/* end of changed path entries */
		if (read_end(c)) malformed_die(c);

		/* rev number */
		rev = read_number(c);
		if (rev <= 0) malformed_die(c);

		/* author */
		if (read_list(c)) malformed_die(c);
		read_string(c, &author);
		if (read_end(c)) malformed_die(c);

		/* timestamp */
		if (read_list(c)) malformed_die(c);
		read_string(c, &time);
		if (read_end(c)) malformed_die(c);

		/* log message */
		if (read_list(c)) malformed_die(c);
		strbuf_reset(&msg);
		append_string(c, &msg, 1);
		strbuf_complete_line(&msg);
		if (read_end(c)) malformed_die(c);
		cmt_read(refs, refnr, (int) rev, author.buf, time.buf, msg.buf);

		if (read_end(c)) malformed_die(c);
		read_newline(c);
	}

	strbuf_release(&name);
	strbuf_release(&author);
	strbuf_release(&time);
	strbuf_release(&msg);
	strbuf_release(&paths);
}







static void read_text_delta(struct conn *c, struct strbuf *d) {
	strbuf_reset(d);
	for (;;) {
		const char* s = read_command(c);

		if (!strcmp(s, "textdelta-end")) {
			if (read_command_end(c)) malformed_die(c);
			return;

		} else if (!strcmp(s, "textdelta-chunk")) {
			/* file-token, chunk */
			if (skip_next(c) || append_string(c, d, 1)) {
				die("invalid textdelta command");
			}
		}

		if (read_command_end(c)) malformed_die(c);
	}
}

/* updates the path to the relative path. empty if its the root */
static void relative_svn_path(struct strbuf *path, int skip) {
	clean_svn_path(path);
	if (path->len == skip) {
		strbuf_reset(path);
	} else {
		strbuf_remove(path, 0, skip);
	}
}

#ifndef NO_PTHREADS
static pthread_mutex_t update_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

static void *update_worker(void *p) {
	struct strbuf name = STRBUF_INIT;
	struct strbuf before = STRBUF_INIT;
	struct strbuf after = STRBUF_INIT;
	struct strbuf diff = STRBUF_INIT;
	struct conn *c = p;

	svn_connect(c, NULL);

	for (;;) {
		int skip = 0;
		int create = -1;
		struct svn_entry *cmt;
		const char *path;

		pthread_mutex_lock(&update_lock);
		cmt = svn_start_next_update();
		pthread_mutex_unlock(&update_lock);

		if (!cmt)
			break;

		path = cmt->ref->path;

		if (cmt->copysrc) {
			/* [rev] target recurse target-url */
			sendf(c, "( switch ( ( %d ) %d:%s true %d:%s%s ) )\n",
					cmt->rev,
					(int) strlen(cmt->copysrc),
					cmt->copysrc,
					(int) (url.len + strlen(path)),
					url.buf,
					path);

			/* path rev start-empty */
			sendf(c, "( set-path ( 0: %d false ) )\n", cmt->copyrev);

			skip = strlen(cmt->copysrc);
		} else {
			/* [rev] target recurse */
			sendf(c, "( update ( ( %d ) %d:%s true ) )\n",
					cmt->rev,
					(int) strlen(path),
					path);

			/* path rev start-empty */
			if (cmt->new_branch) {
				sendf(c, "( set-path ( 0: %d true ) )\n", cmt->rev);
			} else {
				sendf(c, "( set-path ( 0: %d false ) )\n", cmt->rev - 1);
			}

			skip = strlen(path);
		}

		sendf(c, "( finish-report ( ) )\n");

		for (;;) {
			const char *s = read_command(c);

			if (!strcmp(s, "close-edit")) {
				if (read_command_end(c)) malformed_die(c);
				break;

			} else if (!strcmp(s, "abort-edit") || !strcmp(s, "failure")) {
				if (read_command_end(c)) malformed_die(c);
				die("update aborted");

			} else if (!strcmp(s, "add-dir")) {
				/* path, parent-token, child-token, [copy-path, copy-rev] */
				if (read_string(c, &name)) malformed_die(c);
				relative_svn_path(&name, skip);
				helperf(cmt, "add-dir %d:%s\n", (int) name.len, name.buf);
				if (read_command_end(c)) malformed_die(c);

			} else if (!strcmp(s, "open-file")) {
				/* name, dir-token, file-token, rev */
				if (read_string(c, &name)) malformed_die(c);
				if (read_command_end(c)) malformed_die(c);
				relative_svn_path(&name, skip);
				create = 0;

			} else if (!strcmp(s, "add-file")) {
				/* name, dir-token, file-token, [copy-path, copy-rev] */
				if (read_string(c, &name)) malformed_die(c);
				if (read_command_end(c)) malformed_die(c);
				relative_svn_path(&name, skip);
				create = 1;

			} else if (!strcmp(s, "apply-textdelta")) {
				/* file-token, [base-checksum] */
				if (skip_next(c)) malformed_die(c);
				if (have_optional(c)) {
					if (read_string(c, &before)) malformed_die(c);
					if (read_end(c)) malformed_die(c);
				}
				if (read_command_end(c)) malformed_die(c);

				read_text_delta(c, &diff);

			} else if (!strcmp(s, "close-file")) {
				if (create < 0) malformed_die(c);

				/* file-token, [text-checksum] */
				if (skip_next(c)) malformed_die(c);
				if (have_optional(c)) {
					if (read_string(c, &after)) malformed_die(c);
					if (read_end(c)) malformed_die(c);
				}
				if (read_command_end(c)) malformed_die(c);

				/* we need to ignore file changes that only
				 * change the file metadata */
				if (diff.len) {
					helperf(cmt, "%s %d:%s %d:%s %d:%s %d:",
							create ? "add-file" : "open-file",
							(int) name.len, name.buf,
							(int) before.len, before.buf,
							(int) after.len, after.buf,
							(int) diff.len);

					write_helper(cmt, diff.buf, diff.len, 1);
				}

				strbuf_release(&diff);
				strbuf_reset(&before);
				strbuf_reset(&after);
				create = -1;

			} else if (!strcmp(s, "delete-entry")) {
				/* name, [revno], dir-token */
				if (read_string(c, &name)) malformed_die(c);
				if (read_command_end(c)) malformed_die(c);
				relative_svn_path(&name, skip);

				if (name.len) {
					helperf(cmt, "delete-entry %d:%s\n",
							(int) name.len,
							name.buf);
				}

			} else {
				if (read_command_end(c)) malformed_die(c);
			}
		}

		if (create >= 0) malformed_die(c);

		sendf(c, "( success ( ) )\n");

		pthread_mutex_lock(&update_lock);
		svn_finish_update(cmt);
		pthread_mutex_unlock(&update_lock);
	}

	strbuf_release(&name);
	strbuf_release(&before);
	strbuf_release(&after);
	strbuf_release(&diff);

	return NULL;
}

static void svn_read_updates(int cmts) {
#ifndef NO_PTHREADS
	int i, nr = min(cmts, svn_max_requests) - 1;
	pthread_t *threads = malloc(nr * sizeof(threads[0]));
	struct conn *conns = malloc(nr * sizeof(conns[0]));
	for (i = 0; i < nr; i++) {
		init_connection(&conns[i]);
		pthread_create(&threads[i], NULL, &update_worker, &conns[i]);
	}
#endif

	(void) cmts;
	update_worker(&main_connection);

#ifndef NO_PTHREADS
	for (i = 0; i < nr; i++) {
		pthread_join(threads[i], NULL);
		reset_connection(&conns[i]);
	}
	free(threads);
	free(conns);
#endif
}



static struct strbuf cpath = STRBUF_INIT;
static int cdepth;

static int change_dir(const char* path) {
	const char *p;
	int depth = 0;
	struct conn *c = &main_connection;
	int off = common_directory(path, cpath.buf, cpath.len, &depth);

	/* cd .. to the common root */
	while (cdepth > depth) {
		sendf(c, "( close-dir ( 3:d%02X ) )\n", cdepth);
		cdepth--;
	}

	strbuf_setlen(&cpath, off);

	/* cd down to the new path */
	p = path + off;
	while (*p == '/') {
		const char *dir = p;
		p = strchr(dir+1, '/');
		if (!p) break;

		sendf(c, "( open-dir ( %d:%.*s 3:d%02X 3:d%02X ( ) ) )\n",
			(int) (p - (path+1)), (int) (p - (path+1)), path+1,
			cdepth, cdepth+1);

		strbuf_add(&cpath, dir, p - dir);
		cdepth++;
	}

	return cdepth;
}

static void dir_changed(int dir, const char* path) {
	strbuf_reset(&cpath);
	strbuf_addstr(&cpath, path);
	cdepth = dir;
}

static void svn_delete(const char *p) {
	struct conn *c = &main_connection;
	int dir = change_dir(p);
	sendf(c, "( delete-entry ( %d:%s ( ) 3:d%02X ) )\n",
			(int) strlen(p+1), p+1, dir);
}

static void svn_start_commit(int type, const char *log, const char *path, int rev, const char *copy, int copyrev) {
	struct conn *c = &main_connection;
	int dir;

	sendf(c, "( commit ( %d:%s ) )\n", (int) strlen(log), log);
	sendf(c, "( target-rev ( %d ) )\n", rev);
	sendf(c, "( open-root ( ( ) 3:d00 ) )\n");

	if (read_success(c) || read_success(c))
		die("start commit failed");

	dir = change_dir(path);

	if (type == SVN_DELETE || type == SVN_REPLACE) {
		svn_delete(path);
	}

	if (copyrev && (type == SVN_ADD || type == SVN_REPLACE)) {
		sendf(c, "( add-dir ( %d:%s 3:d%02X 3:d%02X ( %d:%s%s %d ) ) )\n",
				(int) strlen(path+1), path + 1,
				dir, dir+1,
				(int) (url.len + strlen(copy)),
				url.buf, copy,
				copyrev);

		dir_changed(++dir, path);

	} else if (*path && type != SVN_DELETE) {
		sendf(c, "( %s ( %d:%s 3:d%02X 3:d%02X ( ) ) )\n",
			type == SVN_ADD ? "add-dir" : "open-dir",
			(int) strlen(path+1), path + 1,
			dir, dir + 1);

		dir_changed(++dir, path);
	}
}

static void svn_set_mergeinfo(const char *path, struct mergeinfo *mi) {
	struct conn *c = &main_connection;
	int dir = change_dir(path);
	const char *str = make_svn_mergeinfo(mi);
	sendf(c, "( change-dir-prop ( 3:d%02X 13:svn:mergeinfo ( %d:%s ) ) )\n",
		dir, (int) strlen(str), str);
}

static int svn_finish_commit(struct strbuf *time) {
	struct conn *c = &main_connection;
	int rev;

	change_dir("");
	sendf(c, "( close-dir ( 3:d00 ) )\n");
	sendf(c, "( close-edit ( ) )\n");
	if (read_success(c)) return -1;
	if (read_success(c)) return -1;

	/* commit-info: ( new-rev:number date:string author:string ? ( post-commit-err:string ) ) */
	if (read_list(c)) malformed_die(c);
	rev = (int) read_number(c);
	if (have_optional(c) && time) {
		if (read_string(c, time)) malformed_die(c);
		if (read_end(c)) malformed_die(c);
	}
	if (read_end(c)) malformed_die(c);
	read_newline(c);
	return rev;
}

static void svn_mkdir(const char *p) {
	struct conn *c = &main_connection;
	int dir = change_dir(p);
	sendf(c, "( add-dir ( %d:%s 3:d%02X 3:d%02X ( ) ) )\n",
			(int) strlen(p+1), p+1,
			dir, dir+1);

	dir_changed(++dir, p);
}

static void svn_send_file(const char *path, struct strbuf *diff, int create) {
	struct conn *c = &main_connection;
	int dir = change_dir(path);
	size_t n = 0;

	sendf(c, "( %s ( %d:%s 3:d%02X 1:f ( ) ) )\n",
		create ? "add-file" : "open-file",
		(int) strlen(path)-1, path+1, dir);

	sendf(c, "( apply-textdelta ( 1:f ( ) ) )\n");

	while (n < diff->len) {
		size_t sz = min(diff->len - n, 64*1024);
		strbuf_reset(&c->buf);
		strbuf_addf(&c->buf, "( textdelta-chunk ( 1:f %d:", (int) sz);

		if (svndbg >= 2) {
			struct strbuf dbg = STRBUF_INIT;
			strbuf_addstr(&dbg, c->buf.buf);
			if (sz > 20) {
				strbuf_add(&dbg, diff->buf + n, 20);
				strbuf_addstr(&dbg, "...");
			} else {
				strbuf_add(&dbg, diff->buf + n, sz);
			}
			strbuf_addstr(&dbg, " ) )\n");
			writedebug(c, &dbg, 1);
			strbuf_release(&dbg);
		}

		strbuf_add(&c->buf, diff->buf + n, sz);
		strbuf_addstr(&c->buf, " ) )\n");

		if (write_in_full(c->fd, c->buf.buf, c->buf.len) != c->buf.len)
			die_errno("write");

		n += sz;
	}

	sendf(c, "( textdelta-end ( 1:f ) )\n");
	sendf(c, "( close-file ( 1:f ( ) ) )\n");
}

static int svn_has_change(const char *path, int from, int to) {
	struct conn *c = &main_connection;
	int ret = 0;

	sendf(c, "( log ( ( %d:%s ) " /* (path...) */
		"( %d ) ( %d ) " /* start/end revno */
		"false true " /* changed-paths strict-node */
		"1 ) )\n", /* limit */
		(int) strlen(path),
		path,
		to, /* log end */
		from /* log start */
	     );

	if (read_success(c))
		die("log failed");

	while (!read_list(c)) {
		ret = 1;
		if (read_end(c)) malformed_die(c);
	}

	if (read_done(c)|| read_success(c))
		die("log failed");

	return ret;
}

static void svn_disconnect(void) {
	reset_connection(&main_connection);
}



struct svn_proto proto_svn = {
	&svn_get_latest,
	&svn_list,
	&svn_isdir,
	&svn_read_log,
	&svn_read_updates,
	&svn_get_mergeinfo,
	&svn_start_commit,
	&svn_finish_commit,
	&svn_set_mergeinfo,
	&svn_mkdir,
	&svn_send_file,
	&svn_delete,
	&svn_change_user,
	&svn_has_change,
	&svn_disconnect,
};

struct svn_proto* svn_proto_connect(struct strbuf *purl, struct credential *cred, struct strbuf *uuid) {
	struct conn *c = &main_connection;
	svn_auth = cred;
	strbuf_reset(&url);
	strbuf_add(&url, purl->buf, purl->len);

	user_agent = git_user_agent();

	init_connection(&main_connection);
	svn_connect(c, uuid);

	strbuf_reset(purl);
	strbuf_add(purl, url.buf, url.len);

	return &proto_svn;
}
