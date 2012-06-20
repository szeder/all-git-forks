#include "git-compat-util.h"
#include "parse-options.h"
#include "gettext.h"
#include "cache.h"
#include "cache-tree.h"
#include "refs.h"

#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static const char *head;
static const char *refspec;
static const char *user;
static const char *pass;
static const char *tmpname;
static struct index_state* istate = &the_index;
static int verbose;

static const char * const builtin_svn_sync_usage[] = {
	"git svn-sync [options] <repository> <head> <refspec>",
	NULL,
};

static struct option builtin_svn_sync_options[] = {
	OPT_STRING(0, "user", &user, "user", "svn username"),
	OPT_STRING(0, "pass", &pass, "pass", "svn password"),
	OPT_STRING(0, "tmp", &tmpname, "tmp", "temporary file location"),
	OPT_BOOLEAN('v', "verbose", &verbose, "verbose logging of all svn traffic"),
	OPT_END()
};

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define xdie fprintf(stderr, "\n%d: ", __LINE__), die

#if 0
static int xgetchar2(int lineno) {
	int ch = getchar();
	fprintf(stderr, "%d %d %c\n", lineno, ch, ch);
	return ch;
}
#define xgetchar() xgetchar2(__LINE__)
#else
#define xgetchar() getchar()
#endif

static int fseek64(FILE* f, int64_t off, int whence) {
	return fseeko(f, off, whence);
}

static int readf(void* u, void* p, int n) {
	return fread(p, 1, n, (FILE*) u);
}

static int writef(void* u, const void* p, int n) {
	return fwrite(p, 1, n, (FILE*) u);
}

typedef int (*reader)(void*, void*, int);
typedef int (*writer)(void*, const void*, int);

struct buf {
	char* p;
	int n;
};

static int readb(void* u, void* p, int n) {
	struct buf* b = u;
	int r = min(b->n, n);
	memcpy(p, b->p, r);
	b->n -= r;
	b->p += r;
	return r;
}

static int writeb(void* u, const void* p, int n) {
	struct buf* b = u;
	int r = min(b->n, n);
	memcpy(b->p, p, r);
	b->n -= r;
	b->p += r;
	return r;
}

static int tohex(int v) {
	return (v >= 10) ? (v - 10 + 'a') : (v + '0');
}

struct ascii_writer {
	writer f;
	void* d;
};

static int write_ascii(void* u, const void* p, int n) {
	int i;
	struct ascii_writer* w = u;
	const unsigned char* v = p;

	for (i = 0; i < n; i++) {
		int ch = v[i];

		if (' ' <= ch && ch < 0x7F && ch != '\\') {
			if (w->f(w->d, &v[i], 1) != 1) {
				return -1;
			}
		} else if (ch == '\n') {
			if (w->f(w->d, "\n", 2) != 2) {
				return -1;
			}
		} else if (ch == '\r') {
			if (w->f(w->d, "\r", 2) != 2) {
				return -1;
			}
		} else if (ch == '\t') {
			if (w->f(w->d, "\t", 2) != 2) {
				return -1;
			}
		} else if (ch == '\\') {
			if (w->f(w->d, "\\", 2) != 2) {
				return -1;
			}
		} else {
			char b[4];
			b[0] = '\\';
			b[1] = 'x';
			b[2] = tohex(ch >> 4);
			b[3] = tohex(ch & 0x0F);
			if (w->f(w->d, b, 4) != 4) {
				return -1;
			}
		}
	}

	return n;
}

static void print_hex(writer wf, void* wd, const void* p, int n) {
	int i;
	const unsigned char* v = p;
	for (i = 0; i < n; i++) {
		char b[2];
		b[0] = tohex(v[i] >> 4);
		b[1] = tohex(v[i] & 0x0F);
		wf(wd, b, 2);
	}
}

static void copyn(writer wf, void* wd, reader rf, void *rd, int64_t n) {
	while (n > 0) {
		char buf[BUFSIZ];
		int r = rf(rd, buf, min(sizeof(buf), n));
		if (r <= 0)
			xdie(_("unexpected end %s"), strerror(errno));

		if (wf && wf(wd, buf, r) != r)
			xdie(_("failed write %s"), strerror(errno));

		n -= r;
	}
}

static void readfull(void *p, reader rf, void* rd, int n) {
	while (n > 0) {
		int r = rf(rd, p, n);
		if (r <= 0)
			xdie(_("unexpected end %s"), strerror(errno));

		p = (char*) p + r;
		n -= r;
	}
}

static int64_t read_varint(reader rf, void* rd) {
	int64_t v = 0;
	unsigned char ch = 0x80;

	while (ch & 0x80) {
		if (v > (INT64_MAX >> 7) || rf(rd, &ch, 1) != 1)
			xdie(_("invalid svndiff"));

		v = (v << 7) | (ch & 0x7F);
	}

	return v;
}

static int writesvn(const char* fmt, ...) {
	va_list ap, aq;
	va_start(ap, fmt);
	if (verbose) {
		va_copy(aq, ap);
		fprintf(stderr, "+");
		vfprintf(stderr, fmt, aq);
	}
	vfprintf(stdout, fmt, ap);
	return 0;
}

#define COPY_FROM_SOURCE (0 << 6)
#define COPY_FROM_TARGET (1 << 6)
#define COPY_FROM_NEW    (2 << 6)

static int decode_instruction(reader rf, void* rd, int64_t* off, int64_t* len) {
	unsigned char hdr;
	int ret;

	if (rf(rd, &hdr, 1) != 1)
		xdie(_("invalid svndiff"));

	*len = hdr & 0x3F;
	if (*len == 0) {
		*len = read_varint(rf, rd);
	}

	ret = hdr & 0xC0;
	if (ret == COPY_FROM_SOURCE || ret == COPY_FROM_TARGET) {
		*off = read_varint(rf, rd);
	} else {
		*off = 0;
	}

	return ret;
}

static void apply_svndiff0_win(FILE* tgt, FILE* src, reader df, void* dd) {
	char insv[4096];
	char tgtv[4096];
	struct buf ins;
	int64_t srco = read_varint(df, dd);
	int64_t srcl = read_varint(df, dd);
	int64_t tgtl = read_varint(df, dd);
	int64_t insl = read_varint(df, dd);
	int64_t datal = read_varint(df, dd);
	int64_t w = 0;

	if (insl > sizeof(insv)) goto err;
	readfull(insv, df, dd, (int) insl);
	ins.p = insv;
	ins.n = insl;

	while (ins.n) {
		int64_t off, len;
		int tgtr;
		switch (decode_instruction(&readb, &ins, &off, &len)) {
		case COPY_FROM_SOURCE:
			if (!src) goto err;
			if (off + len > srcl) goto err;
			fseek64(src, srco + off, SEEK_SET);
			copyn(&writef, tgt, &readf, src, len);
			w += len;
			break;

		case COPY_FROM_TARGET:
			tgtr = min(w - off, len);
			if (tgtr <= 0 || tgtr > sizeof(tgtv)) goto err;
			fseek64(tgt, -w + off, SEEK_END);
			readfull(tgtv, &readf, tgt, tgtr);
			len -= tgtr;

			/* The target len may roll off the end, in that
			 * case we just copy out what is in the buffer.
			 * This is used for repeats.
			 */
			while (len) {
				int n = min(len, tgtr);
				if (fwrite(tgtv, 1, n, tgt) != n)
					goto err;
				len -= n;
				w += n;
			}
			break;

		case COPY_FROM_NEW:
			if (len > datal) goto err;
			copyn(&writef, tgt, df, dd, len);
			w += len;
			datal -= len;
			break;

		default:
			goto err;
		}
	}

	if (w != tgtl || datal) goto err;

	return;
err:
	xdie(_("invalid svndiff"));
}

static void apply_svndiff0(FILE* tgt, FILE* src, reader df, void* dd, int (*eof)(void*)) {
	char hdr[4];
	if (df(dd, hdr, 4) != 4) die("tmp");
	if (memcmp(hdr, "SVN\0", 4))
		goto err;

	while (!eof(dd)) {
		apply_svndiff0_win(tgt, src, df, dd);
	}

	return;

err:
	xdie(_("invalid svndiff"));
}

/* returns -1 if it can't find a number */
static int64_t read_number64() {
	int64_t v;

	for (;;) {
		int ch = xgetchar();
		if ('0' <= ch && ch <= '9') {
			v = ch - '0';
			break;
		} else if (ch != ' ' && ch != '\n') {
			ungetc(ch, stdin);
			return -1;
		}
	}

	for (;;) {
		int ch = xgetchar();
		if (ch < '0' || ch > '9') {
			ungetc(ch, stdin);
			if (verbose) fprintf(stderr, " %d", (int) v);
			return v;
		}

		if (v > INT64_MAX/10) {
			xdie(_("number too big"));
		} else {
			v = 10*v + (ch - '0');
		}
	}
}

static int read_number() {
	int64_t v = read_number64();
	if (v > INT_MAX)
		xdie(_("number too big"));
	return (int) v;
}

/* returns -1 if it can't find a list */
static int read_list() {
	for (;;) {
		int ch = xgetchar();
		if (ch == '(') {
			if (verbose) fprintf(stderr, " (");
			return 0;
		} else if (ch != ' ' && ch != '\n') {
			ungetc(ch, stdin);
			return -1;
		}
	}
}

/* returns 0 if the list is missing or empty (and skips over it), 1 if
 * its present and has values */
static int have_optional() {
	if (read_list()) return 0;
	for (;;) {
		int ch = xgetchar();
		if (ch == ')') {
			if (verbose) fprintf(stderr, " )");
			return 0;
		} else if (ch != ' ' && ch != '\n') {
			ungetc(ch, stdin);
			return 1;
		}
	}
}

/* returns NULL if it can't find an atom, string only valid until next
 * call to read_word, not thread-safe */
static const char *read_word() {
	static char buf[256];
	int bufsz = 0;
	int ch;

	for (;;) {
		ch = xgetchar();
		if (('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z')) {
			break;
		} else if (ch != ' ' && ch != '\n') {
			ungetc(ch, stdin);
			return NULL;
		}
	}

	while (('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z')
			|| ('0' <= ch && ch <= '9')
			|| ch == '-') {
		if (bufsz >= sizeof(buf))
			xdie(_("atom too long"));

		buf[bufsz++] = ch;
		ch = xgetchar();
	}

	ungetc(ch, stdin);
	buf[bufsz] = '\0';
	if (verbose) fprintf(stderr, " %s", buf);
	return bufsz ? buf : NULL;
}

/* reads the string header, returning number of bytes to read from stdin
 * afterwards or -1 if no string can be found */
static int64_t read_string_size() {
	int64_t sz = read_number64();
	if (sz < 0)
		return sz;
	if (xgetchar() != ':')
		xdie(_("malformed string"));
	if (verbose) fprintf(stderr, ":");
	return sz;
}

static int read_strbuf(struct strbuf* s) {
	int64_t n = read_string_size();
	if (n < 0) return -1;

	if (strbuf_fread(s, n, stdin) != n) {
		die_errno("malformed string");
	}
	return 0;
}

static int read_string(writer wf, void* wd) {
	int64_t n = read_string_size();
	if (n < 0) return -1;

	copyn(wf, wd, &readf, stdin, n);
	return 0;
}

static void read_end() {
	int parens = 1;
	while (parens > 0) {
		int ch = xgetchar();
		if (ch == EOF)
			xdie(_("socket close whilst looking for list close"));

		if (ch == '(') {
			if (verbose) fprintf(stderr, " (");
			parens++;
		} else if (ch == ')') {
			if (verbose) fprintf(stderr, " )");
			parens--;
		} else if (ch == ' ' || ch == '\n') {
			/* whitespace */
		} else if ('0' <= ch && ch <= '9') {
			/* number or string */
			int64_t n;
			ungetc(ch, stdin);
			n = read_number64();

			ch = xgetchar();
			if (ch != ':') {
				/* number */
				ungetc(ch, stdin);
				continue;
			}

			/* string */
			if (verbose) {
				struct ascii_writer w = {&writef, stderr};
				fprintf(stderr, ":");
				copyn(&write_ascii, &w, &readf, stdin, n);
			} else {
				copyn(NULL, NULL, &readf, stdin, n);
			}
		} else {
			ungetc(ch, stdin);
			if (!read_word())
				xdie(_("unexpected character %c"), ch);
		}
	}
}


/* if error, parses full error and returns error code of the first
 * error, if success returns 0 and leaves the params open, use
 * end_response when done, -1 is returned on no error or an app-err of 0
 */
static int read_response() {
	const char *suc;
	int err = 0;

	if (read_list())
		goto error;

	suc = read_word();
	if (!strcmp(suc, "success")) {
		if (read_list())
			goto error;

		return 0;
	}

	if (strcmp(suc, "failure"))
		goto error;

	/* error list */
	if (read_list())
		goto error;

	/* read first error if there is one */
	if (!read_list()) {
		err = read_number();
		read_end();
	}

	/* error list and response */
	read_end();
        read_end();
	if (verbose) fprintf(stderr, "\n");

	/* don't allow a 0 app-err to leak through */
	return err ? err : -1;
error:
	xdie(_("malformed response"));
}

static void read_end_response() {
	read_end();
	read_end();
	if (verbose) fprintf(stderr, "\n");
}

static int read_success() {
	int err = read_response();
	if (err) return err;
	read_end_response();
	return 0;
}

/* most commands seem to have an empty success response first */
static int read_response2() {
	int err = read_success();
	if (err) return err;
	return read_response();
}

static int read_success2() {
	int err = read_success();
	if (err) return err;
	return read_success();
}

static void cram_md5() {
	const char *s;
	char chlg[256];
	unsigned char hash[16];
	char hb[32];
	int64_t sz;
	HMAC_CTX hmac;
	struct buf b;

	if (read_list()) goto error;

	s = read_word();
	if (!s || strcmp(s, "step")) goto error;

	if (read_list()) goto error;

	sz = read_string_size();
	if (sz < 0 || sz >= sizeof(chlg)) goto error;
	readfull(chlg, &readf, stdin, (int) sz);

	/* finish off the step */
	read_end();
	read_end();
	if (verbose) fprintf(stderr, "\n");

	HMAC_Init(&hmac, (unsigned char*) pass, strlen(pass), EVP_md5());
	HMAC_Update(&hmac, (unsigned char*) chlg, sz);
	HMAC_Final(&hmac, hash, NULL);
	HMAC_CTX_cleanup(&hmac);

	b.p = hb;
	b.n = sizeof(hb);
	print_hex(&writeb, &b, hash, sizeof(hash));
	writesvn("%d:%s %.*s\n", (int) strlen(user) + 1 + 32, user, 32, hb);

	return;

error:
	xdie(_("auth failed"));
}

static int64_t deltamore() {
	for (;;) {
		const char* s;
		int64_t n;

		/* finish off the previous textdelta-chunk or
		 * apply-textdelta */
		read_end(); /* params */
		read_end(); /* cmd */
		if (verbose) fprintf(stderr, "\n");

		/* cmd */
		if (read_list()) goto err;

		s = read_word();
		if (!s) goto err;

		/* params */
		if (read_list()) goto err;

		if (!strcmp(s, "textdelta-end")) {
			return 0;
		}

		/* if we get some other command we just loop around
		 * again */
		if (strcmp(s, "textdelta-chunk")) {
			continue;
		}

		if (read_string(NULL, NULL)) goto err;

		n = read_string_size();
		if (n < 0) goto err;
		if (n > 0) return n;
	}

err:
	die("invalid textdelta command");
}

static int deltaeof(void* u) {
	return (*(int64_t*) u) <= 0;
}

static int deltar(void* u, void* p, int n) {
	int64_t* d = u;
	int r;

	if (*d <= 0) return *d;

	r = fread(p, 1, min(n, *d), stdin);
	*d -= r;

	if (verbose) print_hex(&writef, stderr, p, r);

	if (*d == 0) *d = deltamore();

	return r;
}

static void read_name(struct strbuf* name) {
	strbuf_reset(name);
	if (read_strbuf(name)) goto err;
	if (name->buf[0] == '/') goto err;
	if (memchr(name->buf, '\0', name->len)) goto err;
	if (strstr(name->buf, "//")) goto err;
	if (!strncmp(name->buf, "../", 3)) goto err;
	if (!strncmp(name->buf, "./", 2)) goto err;
	if (strstr(name->buf, "/../")) goto err;
	if (strstr(name->buf, "/./")) goto err;
	if (name->len >= 3 && !memcmp(name->buf + name->len - 3, "/..", 3)) goto err;
	if (name->len >= 2 && !memcmp(name->buf + name->len - 2, "/.", 2)) goto err;

	return;
err:
	xdie("invalid path name %s", name->buf);
}

static void recursive_remove(struct strbuf* name) {
	struct stat st;

	if (stat(name->buf, &st)) goto err;

	if (S_ISDIR(st.st_mode)) {
		struct dirent* ent;
		DIR* dir = opendir(name->buf);
		size_t len = name->len;

		if (!dir) goto err;

		strbuf_addch(name, '/');

		while ((ent = readdir(dir)) != NULL) {
			if (strcmp(ent->d_name, ".") && strcmp(ent->d_name, "..")) {
				strbuf_addstr(name, ent->d_name);
				recursive_remove(name);
			}
		}

		strbuf_setlen(name, len);

		if (rmdir(name->buf)) goto err;
	} else {
		if (unlink(name->buf)) goto err;
		remove_file_from_index(istate, name->buf);
	}

	return;

err:
	die_errno("remove failed on '%s'", name->buf);
}

static void read_update() {
	FILE* src = NULL;
	FILE* tgt = NULL;
	struct strbuf name = STRBUF_INIT;

	if (read_success()) goto err; /* update */
	if (read_success()) goto err; /* report */

	for (;;) {
		const char* cmd;

		if (read_list()) goto err;

		cmd = read_word();
		if (read_list()) goto err;

		if (!strcmp(cmd, "close-edit")) {
			if (tgt) goto err;
			break;

		} else if (!strcmp(cmd, "abort-edit")) {
			xdie(_("update aborted"));

		} else if (!strcmp(cmd, "open-dir")) {
			if (tgt) goto err;

			/* name, parent-token, child-token */
			read_name(&name);

		} else if (!strcmp(cmd, "add-dir")) {
			if (tgt) goto err;

			/* name, parent-token, child-token */
			read_name(&name);

			if (mkdir(name.buf, 0777))
				die_errno("mkdir '%s'", name.buf);

		} else if (!strcmp(cmd, "open-file")) {
			if (tgt) goto err;

			/* name, dir-token, file-token */
			read_name(&name);

			src = fopen(name.buf, "rb");
			if (!src)
				die_errno("open '%s'", name.buf);

			tgt = fopen(tmpname, "wb+x");
			if (!tgt)
				die_errno("open '%s'", tmpname);

		} else if (!strcmp(cmd, "add-file")) {
			if (tgt) goto err;

			/* name, dir-token, file-token */
			read_name(&name);

			tgt = fopen(tmpname, "wb+x");
			if (!tgt)
				die_errno("create '%s'", tmpname);

		} else if (!strcmp(cmd, "close-file")) {
			if (!tgt) goto err;

			fclose(tgt);
			tgt = NULL;

			if (src) {
				fclose(src);
				src = NULL;
			}

			if (rename(tmpname, name.buf))
				die_errno("rename '%s' to '%s'", tmpname, name.buf);

			if (add_file_to_index(istate, name.buf, 0))
				die("add failed on '%s'", name.buf);

		} else if (!strcmp(cmd, "delete-entry")) {
			if (tgt) goto err;

			/* name, [revno], dir-token */
			read_name(&name);

			recursive_remove(&name);

		} else if (!strcmp(cmd, "apply-textdelta")) {
			int64_t d;
			if (!tgt) goto err;

			d = deltamore();
			if (d > 0) {
				apply_svndiff0(tgt, src, &deltar, &d, &deltaeof);
			}
		}

		read_end(); /* params */
		read_end(); /* command */
		if (verbose) fprintf(stderr, "\n");
	}

	read_end_response(); /* end of close-edit */
	if (read_success()) goto err;

	strbuf_release(&name);
	return;

err:
	xdie("malformed update");
}

static void get_commit(int rev, unsigned char* sha1) {
	int newfd;
	const char* s;
	struct lock_file index_lock;
	struct ref_lock* ref_lock;
	struct strbuf author = STRBUF_INIT;
	struct strbuf cmt = STRBUF_INIT;
	struct strbuf time = STRBUF_INIT;
	struct tm tm;

	fprintf(stderr, "commit start %d\n", rev);

	writesvn("( update ( ( %d ) 0: true ) )\n", rev);
	if (rev == 1) {
		writesvn("( set-path ( 0: 1 true ) )\n");
	} else {
		writesvn("( set-path ( 0: %d false ) )\n", rev - 1);
	}
	writesvn("( finish-report ( ) )\n");
	writesvn("( success ( ) )\n");
	writesvn("( log ( ( 0: ) " /* path */
			"( %d ) ( %d ) " /* start/end revno */
			"false false " /* changed-paths strict-node */
			"0 " /* limit */
			"false " /* include-merged-revisions */
			"revprops ( 10:svn:author 8:svn:date 7:svn:log ) "
			") )\n", rev, rev);
	fflush(stdout);

	if (!istate->cache_tree)
		istate->cache_tree = cache_tree();

	newfd = hold_locked_index(&index_lock, 1);
	read_update();

	if (write_index(istate, newfd) || commit_locked_index(&index_lock))
		die("unable to write new index file");
	if (cache_tree_update(istate->cache_tree, istate->cache, istate->cache_nr, 0))
		xdie("failed to update cache tree");

	strbuf_init(&cmt, 8192);
	strbuf_addf(&cmt, "tree %s\n", sha1_to_hex(istate->cache_tree->sha1));
	if (rev > 1) {
		strbuf_addf(&cmt, "parent %s\n", sha1_to_hex(sha1));
	}

	/* log response */
	if (read_success()) xdie("log failed");
	if (!have_optional()) xdie("log failed");

	/* changed path entries */
	if (have_optional()) read_end();

	/* revno */
	if (read_number() < 0) goto err;

	/* author */
	if (!have_optional()) goto err;
	read_strbuf(&author);
	strbuf_addstr(&author, " <svn@example.com>");
	read_end();

	/* timestamp */
	if (!have_optional()) goto err;
	if (read_strbuf(&time)) goto err;
	if (!strptime(time.buf, "%Y-%m-%dT%H:%M:%S", &tm)) goto err;
	strbuf_addch(&author, ' ');
	strbuf_addf(&author, "%"PRId64" +0000", (int64_t) mktime(&tm));
	read_end();

	strbuf_addf(&cmt, "author %s\n", author.buf);
	strbuf_addf(&cmt, "committer %s\n", author.buf);

	/* log message */
	if (!have_optional()) goto err;
	strbuf_addch(&cmt, '\n');
	if (read_strbuf(&cmt)) goto err;
	strbuf_complete_line(&cmt);
	read_end();

	read_end();
	if (verbose) fprintf(stderr, "\n");

	if ((s = read_word()) == NULL || strcmp(s, "done")) goto err;
	if (read_success()) goto err;

	ref_lock = lock_any_ref_for_update("HEAD", rev > 1 ? sha1 : NULL, 0);
	if (!ref_lock)
		die("failed to grab ref lock");

	if (write_sha1_file(cmt.buf, cmt.len, "commit", sha1))
		die("failed to create commit");

	if (write_ref_sha1(ref_lock, sha1, "svn update"))
		die("cannot update HEAD ref");

	strbuf_release(&author);
	strbuf_release(&cmt);
	strbuf_release(&time);
	fprintf(stderr, "finished commit %d\n", rev);
	return;
err:
	xdie(_("malformed commit"));
}

int cmd_svn_sync(int argc, const char **argv, const char *prefix)
{
	const char *repo, *refspec;
	unsigned char sha1[20];
	int i, rev;

	argc = parse_options(argc, argv, prefix, builtin_svn_sync_options,
		       	builtin_svn_sync_usage, 0);

	if (argc != 3)
		usage_msg_opt(argc > 3 ? _("Too many arguments.") : _("Too few arguments"),
			builtin_svn_sync_usage, builtin_svn_sync_options);

	if (!tmpname || tmpname[0] != '/') {
		xdie(_("tmp file name must be specified and absolute"));
	}

	freopen(NULL, "wb", stdout);
	freopen(NULL, "rb", stdin);

	repo = argv[0];
	head = argv[1];
	refspec = argv[2];

	if (strncmp(repo, "svn://", strlen("svn://")) != 0)
		xdie(_("only svn repositories are supported"));

	/* TODO: client software version and client capabilities */
	writesvn("( 2 ( edit-pipeline ) %d:%s )\n", (int) strlen(repo), repo);
	writesvn("( CRAM-MD5 ( ) )\n");
	fflush(stdout);

	/* TODO: we don't care about capabilities/versions right now */
	if (read_response()) xdie(_("invalid server error"));

	/* minver then maxver */
	if (read_number() > 2 || read_number() < 2)
		xdie(_("version mismatch"));

	read_end_response();

	/* TODO: read the mech lists et all */
	if (read_response()) xdie(_("invalid mech list"));
	read_end_response();

	cram_md5();
	writesvn("( reparent ( %d:%s ) )\n", (int) strlen(repo), repo);
	writesvn("( get-latest-rev ( ) )\n");
	fflush(stdout);

	if (read_success()) xdie(_("auth failed"));
	if (read_success()) xdie(_("no repo info"));
	if (read_success2()) xdie(_("reparent failed"));

	if (read_response2()) xdie(_("get latest rev failed"));
	rev = read_number();
	read_end_response();

	fprintf(stderr, "latest rev %d\n", rev);
	for (i = 1; i <= rev; i++) {
		get_commit(i, sha1);
	}

	return 0;
}
