#include "git-compat-util.h"
#include "parse-options.h"
#include "gettext.h"
#include "cache.h"
#include "cache-tree.h"
#include "refs.h"
#include "unpack-trees.h"
#include "commit.h"
#include "tag.h"
#include "revision.h"
#include "diff.h"
#include "diffcore.h"

#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static const char *pulluser;
static const char *pullpass;
static char *trunk;
static char *branches;
static char *tags;
static const char *revisions;
static int verbose;
static int use_stdin;
static FILE* tmpf;
static int infd = -1;
static int outfd = -1;
static int interval = -1;

static const char* const builtin_svn_fetch_usage[] = {
	"git svn-fetch [options] <repository>",
	NULL,
};

static struct option builtin_svn_fetch_options[] = {
	OPT_STRING(0, "user", &pulluser, "user", "svn username"),
	OPT_STRING(0, "pass", &pullpass, "pass", "svn password"),
	OPT_BOOLEAN('v', "verbose", &verbose, "verbose logging of all svn traffic"),
	OPT_STRING('r', "revision", &revisions, "N:M", "revisions to fetch in the form N or N:M"),
	OPT_BOOLEAN(0, "inetd", &use_stdin, "inetd mode using stdin/out"),
	OPT_STRING('t', "trunk", &trunk, "path", "path of trunk branch"),
	OPT_STRING('b', "branches", &branches, "path", "path of branches"),
	OPT_STRING('T', "tags", &tags, "path", "path of tags"),
	OPT_INTEGER(0, "interval", &interval, "poll interval in seconds (0 to only wake on a signal)"),
	OPT_END()
};

static const char* const builtin_svn_push_usage[] = {
	"git svn-push [options] repo ref <commit>..<commit>",
	NULL,
};

static struct option builtin_svn_push_options[] = {
	OPT_BOOLEAN('v', "verbose", &verbose, "verbose logging of all svn traffic"),
	OPT_STRING('t', "trunk", &trunk, "path", "path of trunk branch"),
	OPT_STRING('b', "branches", &branches, "path", "path of branches"),
	OPT_STRING('T', "tags", &tags, "path", "path of tags"),
	OPT_END()
};

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

static char inbuf[4096];
static int inb, ine;

static int readc() {
	if (ine == inb) {
		inb = 0;
		ine = read(infd, inbuf, sizeof(inbuf));
		if (ine <= 0) return EOF;
	}

	return inbuf[inb++];
}

static void unreadc() {
	inb--;
}

static int readsvn(void* u, void* p, int n) {
	/* big reads we may as well read directly into the target */
	if (ine == inb && n >= sizeof(inbuf) / 2) {
		return read(infd, p, n);

	} else if (ine == inb) {
		inb = 0;
		ine = read(infd, inbuf, sizeof(inbuf));
		if (ine <= 0) return ine;
	}

	n = min(n, ine - inb);
	memcpy(p, &inbuf[inb], n);
	inb += n;
	return n;
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

static const char hex[] = "0123456789abcdef";

static int print_ascii(writer wf, void* wd, const void* p, int n) {
	int i;
	const unsigned char* v = p;

	for (i = 0; i < n; i++) {
		int ch = v[i];

		if (' ' <= ch && ch < 0x7F && ch != '\\') {
			if (wf(wd, &v[i], 1) != 1) {
				return -1;
			}
		} else if (ch == '\n') {
			if (wf(wd, "\\n", 2) != 2) {
				return -1;
			}
		} else if (ch == '\r') {
			if (wf(wd, "\\r", 2) != 2) {
				return -1;
			}
		} else if (ch == '\t') {
			if (wf(wd, "\\t", 2) != 2) {
				return -1;
			}
		} else if (ch == '\\') {
			if (wf(wd, "\\\\", 2) != 2) {
				return -1;
			}
		} else {
			char b[4];
			b[0] = '\\';
			b[1] = 'x';
			b[2] = hex[ch >> 4];
			b[3] = hex[ch & 0x0F];
			if (wf(wd, b, 4) != 4) {
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
		b[0] = hex[v[i] >> 4];
		b[1] = hex[v[i] & 0x0F];
		wf(wd, b, 2);
	}
}

static void copyn(writer wf, void* wd, reader rf, void *rd, int64_t n) {
	while (n > 0) {
		char buf[BUFSIZ];
		int r = rf(rd, buf, min(sizeof(buf), n));
		if (r <= 0)
			die(_("unexpected end %s"), strerror(errno));

		if (wf && wf(wd, buf, r) != r)
			die(_("failed write %s"), strerror(errno));

		n -= r;
	}
}

static void readfull(void *p, reader rf, void* rd, int n) {
	while (n > 0) {
		int r = rf(rd, p, n);
		if (r <= 0)
			die(_("unexpected end %s"), strerror(errno));

		p = (char*) p + r;
		n -= r;
	}
}

static int64_t read_varint(reader rf, void* rd) {
	int64_t v = 0;
	unsigned char ch = 0x80;

	while (ch & 0x80) {
		if (v > (INT64_MAX >> 7) || rf(rd, &ch, 1) != 1)
			die(_("invalid svndiff"));

		v = (v << 7) | (ch & 0x7F);
	}

	return v;
}

#define MAX_VARINT_LEN 9

static unsigned char* encode_varint(unsigned char* p, int64_t n) {
	if (n < 0) die("int too large");
	if (n >= (INT64_C(1) << 56)) *(p++) = ((n >> 56) & 0x7F) | 0x80;
	if (n >= (INT64_C(1) << 49)) *(p++) = ((n >> 49) & 0x7F) | 0x80;
	if (n >= (INT64_C(1) << 42)) *(p++) = ((n >> 42) & 0x7F) | 0x80;
	if (n >= (INT64_C(1) << 35)) *(p++) = ((n >> 35) & 0x7F) | 0x80;
	if (n >= (INT64_C(1) << 28)) *(p++) = ((n >> 28) & 0x7F) | 0x80;
	if (n >= (INT64_C(1) << 21)) *(p++) = ((n >> 21) & 0x7F) | 0x80;
	if (n >= (INT64_C(1) << 14)) *(p++) = ((n >> 14) & 0x7F) | 0x80;
	if (n >= (INT64_C(1) << 7)) *(p++) = ((n >> 7) & 0x7F) | 0x80;
	*(p++) = n & 0x7F;
	return p;
}

__attribute__((format (printf,1,2)))
static void sendf(const char* fmt, ...);

static void sendf(const char* fmt, ...) {
	static struct strbuf out = STRBUF_INIT;
	char* nl;
	va_list ap;
	va_start(ap, fmt);
	strbuf_reset(&out);
	strbuf_vaddf(&out, fmt, ap);

	nl = out.buf;
	while (verbose && nl < out.buf + out.len) {
		char* p = nl;
		nl = memmem(p, out.buf + out.len - p, " )\n( ", 5);
		if (nl) {
			/* not last of multi-line */
			nl += 2;
		} else if (out.buf[out.len-1] == '\n') {
			/* last of multi-line */
			nl = out.buf + out.len - 1;
		} else {
			/* last of incomplete line */
			nl = out.buf + out.len;
		}

		if (nl - p >= 2 && !memcmp(p, "( ", 2)) {
			fputc('+', stderr);
		}

		print_ascii(&writef, stderr, p, nl - p);

		if (nl[0] == '\n') {
			fputc('\n', stderr);
			nl++;
		}
	}

	if (write(outfd, out.buf, out.len) != out.len) {
		die_errno("write");
	}
}

static void sendb(const void* data, size_t sz) {
	if (verbose) {
		print_ascii(&writef, stderr, data, sz);
	}
	if (write(outfd, data, sz) != sz) {
		die_errno("write");
	}
}

struct unzip {
	git_zstream z;
	reader rf;
	void* rd;
	int rn;
	unsigned char buf[4096];
};

static int readz(void* u, void* p, int n) {
	struct unzip* z = u;
	int r;

	z->z.avail_out = n;
	z->z.next_out = p;

	if (z->z.avail_in) {
		r = git_inflate(&z->z, 0);
		if (r) return -1;
		if (z->z.avail_out < n) return n - z->z.avail_out;
	}

	if (z->rn) {
		memmove(z->buf, z->z.next_in, z->z.avail_in);
		z->z.next_in = z->buf;
		r = z->rf(z->rd, z->buf, min(z->rn, sizeof(z->buf) - z->z.avail_in));
		if (r <= 0) return -1;
		z->rn -= r;
		z->z.avail_in += r;
	}

	r = git_inflate(&z->z, z->rn ? 0 : Z_FINISH);
	if (r) return -1;

	return n - z->z.avail_out;
}

#define COPY_FROM_SOURCE (0 << 6)
#define COPY_FROM_TARGET (1 << 6)
#define COPY_FROM_NEW    (2 << 6)

static int read_instruction(reader rf, void* rd, int64_t* off, int64_t* len) {
	unsigned char hdr;
	int ret;

	if (rf(rd, &hdr, 1) != 1)
		die(_("invalid svndiff"));

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

#define MAX_INS_LEN (1 + 2 * MAX_VARINT_LEN)

static unsigned char* encode_instruction(unsigned char* p, int ins, int64_t off, int64_t len) {
	if (len < 0x3F) {
		*(p++) = ins | len;
	} else {
		*(p++) = ins;
		p = encode_varint(p, len);
	}

	if (ins == COPY_FROM_SOURCE || ins == COPY_FROM_TARGET) {
		p = encode_varint(p, off);
	}

	return p;
}

static void apply_svndiff_win(FILE* tgt, const void* srcp, size_t srcn, reader df, void* dd, int ver) {
	struct unzip z;
	char insv[4096];
	char tgtv[4096];
	struct buf ins, b;
	int64_t srco = read_varint(df, dd);
	int64_t srcl = read_varint(df, dd);
	int64_t tgtl = read_varint(df, dd);
	int64_t insc = read_varint(df, dd);
	int64_t datac = read_varint(df, dd);
	int64_t insl = ver > 0 ? read_varint(df, dd) : insc;
	int64_t datal;
	int64_t w = 0;

	if (srco + srcl > srcn) goto err;
	if (insl > sizeof(insv)) goto err;

	if (insc < insl) {
		git_inflate_init(&z.z);
		z.rf = df;
		z.rd = dd;
		z.rn = insc;
		readfull(insv, &readz, &z, (int) insl);
		git_inflate_end(&z.z);
	} else {
		readfull(insv, df, dd, (int) insl);
	}

	ins.p = insv;
	ins.n = insl;

	datal = ver > 0 ? read_varint(df, dd) : datac;

	if (datac < datal) {
		git_inflate_init(&z.z);
		z.rf = df;
		z.rd = dd;
		z.rn = datac;
		df = &readz;
		dd = &z;
	}

	while (ins.n) {
		int64_t off, len;
		int tgtr;
		switch (read_instruction(&readb, &ins, &off, &len)) {
		case COPY_FROM_SOURCE:
			if (off + len > srcl) goto err;
			b.p = (char*) srcp + srco + off;
			b.n = len;
			copyn(&writef, tgt, &readb, &b, len);
			w += len;
			break;

		case COPY_FROM_TARGET:
			tgtr = min(w - off, len);
			if (tgtr <= 0 || tgtr > sizeof(tgtv)) goto err;
			fseek(tgt, -w + off, SEEK_END);
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

	if (dd == &z) {
		git_inflate_end(&z.z);
	}

	if (w != tgtl || datal) goto err;

	return;
err:
	die(_("invalid svndiff"));
}

static void apply_svndiff(FILE* tgt, const void* srcp, size_t srcn, reader df, void* dd, int (*eof)(void*)) {
	unsigned char hdr[4];
	readfull(hdr, df, dd, 4);
	if (memcmp(hdr, "SVN", 3))
		goto err;

	if (hdr[3] > 1)
		goto err;

	while (!eof(dd)) {
		apply_svndiff_win(tgt, srcp, srcn, df, dd, hdr[3]);
	}

	return;

err:
	die(_("invalid svndiff"));
}

/* returns -1 if it can't find a number */
static int64_t read_number64() {
	int64_t v;

	for (;;) {
		int ch = readc();
		if ('0' <= ch && ch <= '9') {
			v = ch - '0';
			break;
		} else if (ch != ' ' && ch != '\n') {
			unreadc();
			return -1;
		}
	}

	for (;;) {
		int ch = readc();
		if (ch < '0' || ch > '9') {
			unreadc();
			if (verbose) fprintf(stderr, " %d", (int) v);
			return v;
		}

		if (v > INT64_MAX/10) {
			die(_("number too big"));
		} else {
			v = 10*v + (ch - '0');
		}
	}
}

static int read_number() {
	int64_t v = read_number64();
	if (v > INT_MAX)
		die(_("number too big"));
	return (int) v;
}

/* returns -1 if it can't find a list */
static int read_list() {
	for (;;) {
		int ch = readc();
		if (ch == '(') {
			if (verbose) fprintf(stderr, " (");
			return 0;
		} else if (ch != ' ' && ch != '\n') {
			unreadc();
			return -1;
		}
	}
}

/* returns 0 if the list is missing or empty (and skips over it), 1 if
 * its present and has values */
static int have_optional() {
	if (read_list()) return 0;
	for (;;) {
		int ch = readc();
		if (ch == ')') {
			if (verbose) fprintf(stderr, " )");
			return 0;
		} else if (ch != ' ' && ch != '\n') {
			unreadc();
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
		ch = readc();
		if (('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z')) {
			break;
		} else if (ch != ' ' && ch != '\n') {
			unreadc();
			return NULL;
		}
	}

	while (('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z')
			|| ('0' <= ch && ch <= '9')
			|| ch == '-') {
		if (bufsz >= sizeof(buf))
			die(_("atom too long"));

		buf[bufsz++] = ch;
		ch = readc();
	}

	unreadc();
	buf[bufsz] = '\0';
	if (verbose) fprintf(stderr, " %s", buf);
	return bufsz ? buf : NULL;
}


/* reads the string header, returning number of bytes to read from svn
 * afterwards or -1 if no string can be found */
static int64_t read_string_size() {
	int64_t sz = read_number64();
	if (sz < 0)
		return sz;
	if (readc() != ':')
		die(_("malformed string"));
	if (verbose) fprintf(stderr, ":");
	return sz;
}

static int read_strbuf(struct strbuf* s) {
	int64_t n = read_string_size();
	if (n < 0 || n > INT_MAX) return -1;

	strbuf_grow(s, n);
	readfull(s->buf + s->len, &readsvn, NULL, n);
	strbuf_setlen(s, s->len + n);

	if (verbose) {
		print_ascii(&writef, stderr, s->buf + s->len - n, n);
	}
	return 0;
}

static void read_end() {
	int parens = 1;
	while (parens > 0) {
		int ch = readc();
		if (ch == EOF)
			die(_("socket close whilst looking for list close"));

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
			char buf[4096];
			unreadc();
			n = read_number64();

			ch = readc();
			if (ch != ':') {
				/* number */
				unreadc();
				continue;
			}

			/* string */
			if (verbose) fputc(':', stderr);
			while (n) {
				int r = readsvn(NULL, buf, min(n, sizeof(buf)));
				if (r <= 0) die_errno("read");
				if (verbose) print_ascii(&writef, stderr, buf, r);
				n -= r;
			}
		} else {
			unreadc();
			if (!read_word())
				die(_("unexpected character %c"), ch);
		}
	}
}


static const char* read_response() {
	const char *cmd;

	if (read_list()) goto err;

	cmd = read_word();
	if (!cmd) goto err;
	if (read_list()) goto err;

	return cmd;
err:
	die(_("malformed response"));
}

static void read_end_response() {
	read_end();
	read_end();
	if (verbose) fprintf(stderr, "\n");
}

static void read_success() {
	const char* s = read_response();
	if (strcmp(s, "success")) {
		verbose = 1;
		read_end();
		die("unexpected failure");
	}
	read_end_response();
}

static void cram_md5(const char* user, const char* pass) {
	const char *s;
	char chlg[256];
	unsigned char hash[16];
	char hb[32];
	int64_t sz;
	HMAC_CTX hmac;
	struct buf b;

	s = read_response();
	if (strcmp(s, "step")) goto error;

	sz = read_string_size();
	if (sz < 0 || sz >= sizeof(chlg)) goto error;
	readfull(chlg, &readsvn, NULL, (int) sz);

	read_end_response();

	HMAC_Init(&hmac, (unsigned char*) pass, strlen(pass), EVP_md5());
	HMAC_Update(&hmac, (unsigned char*) chlg, sz);
	HMAC_Final(&hmac, hash, NULL);
	HMAC_CTX_cleanup(&hmac);

	b.p = hb;
	b.n = sizeof(hb);
	print_hex(&writeb, &b, hash, sizeof(hash));
	sendf("%d:%s %.*s\n", (int) strlen(user) + 1 + 32, user, 32, hb);

	return;

error:
	die(_("auth failed"));
}

static int64_t deltamore() {
	for (;;) {
		const char* s;
		int64_t n;

		/* finish off the previous textdelta-chunk or
		 * apply-textdelta */
		read_end_response();

		s = read_response();

		if (!strcmp(s, "textdelta-end")) {
			return 0;
		}

		/* if we get some other command we just loop around
		 * again */
		if (strcmp(s, "textdelta-chunk")) {
			continue;
		}

		/* file-token, chunk */
		n = read_string_size();
		if (n < 0) goto err;
		copyn(NULL, NULL, &readsvn, NULL, n);

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

	r = readsvn(NULL, p, min(n, *d));
	*d -= r;

	if (verbose) print_ascii(&writef, stderr, p, r);

	if (*d == 0) *d = deltamore();

	return r;
}

static void read_name(struct strbuf* name) {
	strbuf_reset(name);
	if (read_strbuf(name)) goto err;
	if (name->buf[0] == '/') strbuf_remove(name, 0, 1);
	if (memchr(name->buf, '\0', name->len)) goto err;
	if (strstr(name->buf, "//")) goto err;
	if (!strcmp(name->buf, "..")) goto err;
	if (!strcmp(name->buf, ".")) goto err;
	if (!strncmp(name->buf, "../", 3)) goto err;
	if (!strncmp(name->buf, "./", 2)) goto err;
	if (strstr(name->buf, "/../")) goto err;
	if (strstr(name->buf, "/./")) goto err;
	if (name->len >= 3 && !memcmp(name->buf + name->len - 3, "/..", 3)) goto err;
	if (name->len >= 2 && !memcmp(name->buf + name->len - 2, "/.", 2)) goto err;

	return;
err:
	die("invalid path name %s", name->buf);
}

struct dbrec {
	char rev[12];
	char space;
	char cmt[40];
	char nl;
};

struct svnref {
	struct strbuf svn; /* svn root */
	struct strbuf ref; /* ref path */
	struct strbuf db; /* revdb path */
	struct index_state index;
	int rev;
	unsigned int delete : 1;
	unsigned int dirty : 1;
	unsigned int istag : 1;
	unsigned int create : 1;
	unsigned char parent[20]; /* parent to be used when creating the next commit */
	unsigned char sha1[20]; /* current sha1 stored in the ref, may be a commit or tag */
	unsigned char tree[20];
};

static struct svnref** refs;
static size_t refn, refalloc;

static struct dbrec* upper_bound(int rev, struct dbrec* data, size_t num) {
	struct dbrec *first, *p;
	int count, step;

	first = data;
	count = num;
	while (count > 0) {
		p = first;
		step = count/2;
		p += step;
		if (!(rev < atoi(p->rev))) {
			first = ++p;
			count -= step+1;
		} else {
			count = step;
		}
	}

	return first;
}

/* This is generally called with a commit near the tail, but not always
 * and so we do a binary search. This returns the offset at which to
 * place the next revision. In the case of no commit at that exact
 * revision, it will return the commit just before the requested
 * revision. */
static size_t find_rev(int db, int rev, unsigned char* ret) {
	struct dbrec *p, *revs = NULL;
       	struct stat st;
       	size_t sz;

	if (ret) hashcpy(ret, null_sha1);

	if (fstat(db, &st)) die_errno("revdb stat");

	sz = xsize_t(st.st_size);
	if (sz < sizeof(*p)) return 0;

	revs = xmmap(NULL, sz, PROT_READ, MAP_PRIVATE, db, 0);

	/* see if its the after the last commit first */
	p = revs + sz / sizeof(*p) - 1;
	if (rev < atoi(p->rev)) {
		p = upper_bound(rev, revs, sz / sizeof(*p));
		/* the revision is before the start of this history */
		if (p == revs) return 0;
		p--;
	}

	if (ret) get_sha1_hex(p->cmt, ret);

	munmap(revs, sz);
	return (p + 1 - revs) * sizeof(*p);
}

/* This is generally called with a commit near the tail, so we
 * do a linear reverse lookup. This returns the revision or -1 if the
 * commit wasn't found. */
static int find_commit(struct svnref* r, const unsigned char* cmt) {
	struct dbrec *p, *revs = NULL;
	char* hex;
	int ret = -1;
	struct stat st;
	size_t sz;
	int fd;

	fd = open(r->db.buf, O_RDONLY);

	if (fd < 0) die_errno("revdb open");
	if (fstat(fd, &st)) die_errno("revdb stat");

	sz = xsize_t(st.st_size);
	revs = xmmap(NULL, sz, PROT_READ, MAP_PRIVATE, fd, 0);

	hex = sha1_to_hex(cmt);
	p = revs + sz / sizeof(*p);
	while (p >= revs) {
		if (!memcmp(p->cmt, hex, sizeof(p->cmt))) {
			ret = atoi(p->rev);
			goto end;
		}
		p--;
	}

end:
	munmap(revs, sz);
	close(fd);
	return ret;
}

static void append_to_revdb(struct svnref* r, int rev, const unsigned char* sha1) {
	struct lock_file revdb_lock;
	char rec[sizeof(struct dbrec)+1];
	size_t off;
	int fd;

	fd = hold_lock_file_for_append(&revdb_lock, r->db.buf, 0);
	if (fd < 0) die("commit failed");

	off = find_rev(fd, rev, NULL);

	snprintf(rec, sizeof(rec), "%-12d %s\n", rev, sha1_to_hex(sha1));

	if (lseek(fd, off, SEEK_SET) < 0)
		die_errno("revdb seek");
	if (ftruncate(fd, off))
		die_errno("revdb truncate");
	if (write(fd, rec, sizeof(rec)-1) != sizeof(rec)-1)
		die_errno("revdb write");
	if (commit_lock_file(&revdb_lock))
		die("revdb commit");
}

static int is_in_dir(const char* file, const char* dir, const char** rel) {
	size_t sz = strlen(dir);
	if (strncmp(file, dir, sz)) return 0;
	if (file[sz] && file[sz] != '/') return 0;
	if (rel) *rel = file[sz] ? &file[sz+1] : &file[sz];
	return 1;
}

#define REF_MAY_BE_NEW 1
#define REF_UNPACK 2

static struct svnref* find_ref(struct strbuf* name, int flags) {
	int i;
	struct svnref* r;
	const char *a, *b, *c, *d;
	/* names are of the form
	 * branches/foo/...
	 * a        b  c   d
	 */

	for (i = 0; i < refn; i++) {
		r = refs[i];
		if (!r->svn.len) {
			return r;
		}
		if (name->len < r->svn.len) {
			continue;
		}
		if (name->len == r->svn.len && !memcmp(name->buf, r->svn.buf, r->svn.len)) {
			strbuf_setlen(name, 0);
			return r;
		}
		if (name->buf[r->svn.len] != '/') {
			continue;
		}
		if (!memcmp(name->buf, r->svn.buf, r->svn.len)) {
			strbuf_remove(name, 0, r->svn.len + 1);
			return r;
		}
	}

	a = name->buf;
	d = name->buf + name->len;

	if (!trunk && !branches && !tags) {
		r = xcalloc(1, sizeof(*r));
		strbuf_addstr(&r->ref, "refs/heads/master");
		strbuf_addstr(&r->db, git_path("svn/heads/master"));

	} else if (trunk && is_in_dir(a, trunk, &b)) {
		r = xcalloc(1, sizeof(*r));
		strbuf_addstr(&r->svn, trunk);
		strbuf_addstr(&r->ref, "refs/heads/master");
		strbuf_addstr(&r->db, git_path("svn/heads/master"));

		strbuf_remove(name, 0, b - a);

	} else if (branches && is_in_dir(a, branches, &b) && *b) {
		c = memchr(b, '/', d - b);
		if (!c) c = d;

		r = xcalloc(1, sizeof(*r));
		strbuf_add(&r->svn, a, c - a);
		strbuf_addstr(&r->ref, "refs/heads/");
		strbuf_add(&r->ref, b, c - b);
		strbuf_addstr(&r->db, git_path("svn/heads/"));
		strbuf_add(&r->db, b, c - b);

		strbuf_remove(name, 0, c - a);

	} else if (tags && is_in_dir(a, tags, &b) && *b) {
		c = memchr(b, '/', d - b);
		if (!c) c = d;

		r = xcalloc(1, sizeof(*r));
		strbuf_add(&r->svn, a, c - a);
		strbuf_addstr(&r->ref, "refs/tags/");
		strbuf_add(&r->ref, b, c - b);
		strbuf_addstr(&r->db, git_path("svn/tags/"));
		strbuf_add(&r->db, b, c - b);

		strbuf_remove(name, 0, c - a);

		r->istag = 1;

	} else {
		return NULL;
	}

	if (name->buf[0] == '/') {
		strbuf_remove(name, 0, 1);
	}

	/* only root additions create a new ref */
	if ((flags & REF_MAY_BE_NEW) && !name->len) {
		if (!read_ref(r->ref.buf, r->sha1)) {
			die("new ref '%s' already exists", r->ref.buf);
		}
		r->create = 1;
	} else {
		struct tree* tree;
		struct tree_desc desc;
		struct unpack_trees_options op;
		struct object* obj;

		if (read_ref(r->ref.buf, r->sha1))
			die("ref '%s' not found", r->ref.buf);

		/* this is a modified version of parse_tree_indirect
		 * where we grab the commit hash on the way through */
		obj = parse_object(r->sha1);
		for (;;) {
			if (!obj)
				goto err;
			if (obj->type == OBJ_TREE) {
				hashcpy(r->tree, obj->sha1);
				tree = (struct tree*) obj;
				break;
			} else if (obj->type == OBJ_COMMIT) {
				hashcpy(r->parent, obj->sha1);
				obj = &(((struct commit*) obj)->tree->object);
			} else if (obj->type == OBJ_TAG) {
				obj = ((struct tag*) obj)->tagged;
			} else {
				goto err;
			}
			if (!obj->parsed)
				parse_object(obj->sha1);
		}

		if (flags & REF_UNPACK) {
			init_tree_desc(&desc, tree->buffer, tree->size);

			memset(&op, 0, sizeof(op));
			op.head_idx = -1;
			op.src_index = &r->index;
			op.dst_index = &r->index;
			op.index_only = 1;
			op.debug_unpack = 1;

			unpack_trees(1, &desc, &op);

			fprintf(stderr, "\nunpack trees\n");
			for (i = 0; i < r->index.cache_nr; i++) {
				fprintf(stderr, "%s\n", r->index.cache[i]->name);
			}
		}

		r->rev = find_commit(r, r->sha1);

		/* TODO: what do we need to free? */
	}

	fprintf(stderr, "\ncreated ref %d %s %d %s %s\n",
		       	(int) refn, r->ref.buf, r->rev, sha1_to_hex(r->parent), sha1_to_hex(r->sha1));

	ALLOC_GROW(refs, refn + 1, refalloc);
	refs[refn++] = r;
	return r;

err:
	die("invalid ref '%s'", r->ref.buf);
}

static void read_update(int rev) {
	void* srcp = NULL;
	size_t srcn;
	struct strbuf name = STRBUF_INIT;
	struct strbuf srcname = STRBUF_INIT;
	struct svnref* ref = NULL;
	const char* cmd = NULL;

	read_success(); /* update */
	read_success(); /* report */

	for (;;) {
		/* finish off previous command */
		if (cmd) read_end_response();

		if (read_list()) goto err;

		cmd = read_word();
		if (!cmd || read_list()) goto err;

		if (!strcmp(cmd, "close-edit")) {
			break;

		} else if (!strcmp(cmd, "abort-edit")) {
			die("update aborted");

		} else if (!strcmp(cmd, "open-root")) {
			strbuf_reset(&name);
			find_ref(&name, (rev == 1 ? REF_MAY_BE_NEW : 0) | REF_UNPACK);

		} else if (!strcmp(cmd, "add-dir")) {
			/* path, parent-token, child-token */
			read_name(&name);
			find_ref(&name, REF_MAY_BE_NEW | REF_UNPACK);

		} else if (!strcmp(cmd, "open-file")) {
			/* name, dir-token, file-token, rev */
			enum object_type type;
			struct cache_entry* ce;

			read_name(&name);
			ref = find_ref(&name, REF_UNPACK);
			if (!ref || ref->rev >= rev) continue;
			ce = index_name_exists(&ref->index, name.buf, name.len, 0);
			if (!ce) goto err;

			srcp = read_sha1_file(ce->sha1, &type, &srcn);
			if (!srcp || type != OBJ_BLOB) goto err;

			if (fseek(tmpf, 0, SEEK_SET) || ftruncate(fileno(tmpf), 0))
				die_errno("truncate");

		} else if (!strcmp(cmd, "add-file")) {
			/* name, dir-token, file-token, [copy-path, copy-rev] */
			read_name(&name);
			ref = find_ref(&name, REF_UNPACK);
			if (!ref || ref->rev >= rev) continue;
			srcp = NULL;
			srcn = 0;

			if (fseek(tmpf, 0, SEEK_SET) || ftruncate(fileno(tmpf), 0))
				die_errno("truncate");

			if (have_optional()) {
				int64_t rev;
				read_name(&srcname);
				rev = read_number();
				if (rev < 0 || rev > INT_MAX) goto err;
				die("copy %s %d", srcname.buf, (int) rev);
			}

		} else if (!strcmp(cmd, "close-file")) {
			/* file-token, [text-checksum] */
			struct cache_entry* ce;
			unsigned char sha1[20];
			struct stat st;

			if (!ref || ref->rev >= rev) continue;

			if (srcp) {
				free(srcp);
				srcp = NULL;
				srcn = 0;
			}

			fflush(tmpf);
			fseek(tmpf, 0, SEEK_SET);

			if (fstat(fileno(tmpf), &st))
				die_errno("stat temp file");
			if (index_fd(sha1, dup(fileno(tmpf)), &st, OBJ_BLOB, name.buf, HASH_WRITE_OBJECT))
				die_errno("failed to index temp file");

			ce = make_cache_entry(0644, sha1, name.buf, 0, 0);
			if (!ce) die("make_cache_entry failed for path '%s'", name.buf);
			add_index_entry(&ref->index, ce, ADD_CACHE_OK_TO_ADD | ADD_CACHE_OK_TO_REPLACE);
			ref->dirty = 1;

		} else if (!strcmp(cmd, "delete-entry")) {
			int i;

			/* name, [revno], dir-token */
			read_name(&name);
			ref = find_ref(&name, REF_UNPACK);
			if (!ref || ref->rev >= rev) continue;

			ref->dirty = 1;

			if (!name.len) {
				/* delete branch */
				ref->delete = 1;
				continue;
			}

			i = index_name_pos(&ref->index, name.buf, name.len);

			if (i >= 0) {
				/* file */
				remove_index_entry_at(&ref->index, i);
				continue;
			}

			/* directory, index_name_pos returns -first-1
			 * where first is the position the entry would
			 * be added at, and the cache is sorted */
			i = -i-1;

			strbuf_addch(&name, '/');
			for (i = 0; i < ref->index.cache_nr; i++) {
				struct cache_entry* ce = ref->index.cache[i];
				if (ce_namelen(ce) > name.len && !memcmp(ce->name, name.buf, name.len)) {
					break;
				}

				ce->ce_flags |= CE_REMOVE;
			}

			remove_marked_cache_entries(&ref->index);

		} else if (!strcmp(cmd, "apply-textdelta")) {
			/* file-token, [base-checksum] */
			int64_t d;
			if (!ref || ref->rev >= rev) continue;

			d = deltamore();
			if (d > 0) {
				apply_svndiff(tmpf, srcp, srcn, &deltar, &d, &deltaeof);
			}
		}
	}

	read_end_response(); /* end of close-edit */
	read_success();

	free(srcp);
	strbuf_release(&name);
	return;

err:
	die("malformed update");
}

struct author {
	char* user;
	char* pass;
	char* name;
	char* mail;
};

struct author* authors;
size_t authorn, authoralloc;

static char* strip_space(char* p) {
	char* e = p + strlen(p);

	while (*p == ' ' || *p == '\t') {
		p++;
	}

	while (e > p && (e[-1] == ' ' || e[-1] == '\t')) {
		*(--e) = '\0';
	}

	return p;
}

static void parse_authors() {
	char* p;
	struct stat st;
	int fd = open(git_path("svn/authors"), O_RDONLY);
	if (fd < 0 || fstat(fd, &st)) return;

	p = xmalloc(st.st_size + 1);
	if (read(fd, p, st.st_size) != st.st_size)
	       	die("read failed on authors");

	p[st.st_size] = '\0';

	while (p && *p) {
		struct author a;
		char* line = strchr(p, '\n');
		if (line) *(line++) = '\0';

		a.user = p;

		p = strchr(p, '=');
		if (!p) goto nextline; /* empty line */
		*(p++) = '\0';
		a.name = p;

		p = strchr(p, '<');
		if (!p) die("invalid author entry for %s", a.user);
		*(p++) = '\0';
		a.mail = p;

		p = strchr(p, '>');
		if (!p) die("invalid author entry for %s", a.user);
		*(p++) = '\0';
		a.pass = p;

		a.user = strip_space(a.user);
		a.name = strip_space(a.name);
		a.mail = strip_space(a.mail);

		p = strchr(a.user, ':');
		if (p) {
			*p = '\0';
			a.pass = p+1;
		} else {
			a.pass = NULL;
		}

		if (*a.user == '#') {
			/* comment */
		} else {
			ALLOC_GROW(authors, authorn + 1, authoralloc);
			authors[authorn++] = a;
		}

nextline:
		p = line;
	}

	close(fd);
}

static struct author* get_svn_author(const char* username) {
	int i;

	for (i = 0; i < authorn; i++) {
		struct author* a = &authors[i];
		if (!strcasecmp(username, a->user))
		       	return a;
	}

	die("could not find username '%s' in %s\n"
			"Add a line of the form:\n"
			"%s = Full Name <email@example.com>\n",
			username,
			git_path("svn/authors"),
			username);
}

static struct author* get_commit_author(struct commit* cmt) {
	const char *lb, *le, *mb, *me;
	struct strbuf buf = STRBUF_INIT;
	struct author* ret = NULL;
	int i;

	lb = strstr(cmt->buffer, "\ncommitter ");
	if (!lb) lb = strstr(cmt->buffer, "\nauthor ");
	if (!lb) goto err;
	le = strchr(lb+1, '\n');
	if (!le) goto err;
	mb = memchr(lb, '<', le - lb);
	if (!mb) goto err;
	me = memchr(mb, '>', le - mb);
	if (!me) goto err;

	strbuf_add(&buf, mb+1, me - (mb+1));

	for (i = 0; i < authorn; i++) {
		struct author* a = &authors[i];
		if (strcasecmp(buf.buf, a->mail)) continue;
		if (!a->pass) {
			die("need password for user '%s' in %s\n"
				"Add a line of the form:\n"
				"%s:password = Full Name <%s>\n",
				a->user,
				git_path("svn/authors"),
				a->user,
				a->mail);
		}

		ret = a;
		break;
	}

	if (!ret) {
		die("could not find username/password for %s in %s\n"
				"Add a line of the form:\n"
				"username:password = Full Name <%s>\n",
				buf.buf,
				git_path("svn/authors"),
				buf.buf);
	}

	strbuf_release(&buf);
	return ret;

err:
	die("can not find author in commit %s", sha1_to_hex(cmt->object.sha1));
}

static void get_commit(int rev) {
	struct strbuf author = STRBUF_INIT;
	struct strbuf cmt = STRBUF_INIT;
	struct strbuf time = STRBUF_INIT;
	struct strbuf log = STRBUF_INIT;
	struct strbuf name = STRBUF_INIT;
	int i;

	fprintf(stderr, "commit start %d\n", rev);

	sendf("( update ( ( %d ) 0: true ) )\n" /* [rev] target recurse */
		"( set-path ( 0: %d %s ) )\n" /* path rev start-empty */
		"( finish-report ( ) )\n"
		"( success ( ) )\n"
		"( log ( ( 0: ) " /* (path...) */
				"( %d ) ( %d ) " /* start/end revno */
				"true false " /* changed-paths strict-node */
				"0 " /* limit */
				"false " /* include-merged-revisions */
				"revprops ( 10:svn:author 8:svn:date 7:svn:log ) "
			") )\n",
		       	rev, /* update target */
			rev - 1, /* set-path rev */
			rev > 1 ? "false" : "true", /* set-path start-empty */
			rev, /* log start */
			rev /* log end */
		);

	read_update(rev);

	/* log response */
	read_success();
	if (read_list()) goto err;

	/* changed path entries */
	if (read_list()) goto err;
	while (!read_list()) {
		const char* s;
		struct svnref *to, *from;
		int64_t frev;
		int db;

		/* path, A, ( copy-path, copy-rev) */
		read_name(&name);
		to = find_ref(&name, REF_UNPACK);

		s = read_word();
		if (!s) goto err;

		if (!name.len && !strcmp(s, "A") && have_optional()) {
			read_name(&name);
			from = find_ref(&name, REF_UNPACK);
			if (name.len) die("copy from non-root path");

			frev = read_number();
			if (frev < 0 || frev > INT_MAX) goto err;

			db = open(from->db.buf, O_RDONLY);
			if (db < 0) die_errno("open parent revdb '%s'", from->db.buf);
			find_rev(db, frev, to->parent);
			close(db);

			read_end();
		}

		read_end();
	}
	read_end();

	/* revno */
	if (read_number() != rev) goto err;

	/* author */
	{
		struct author* a;
		if (read_list()) goto err;
		read_strbuf(&author);
		a = get_svn_author(author.buf);
		strbuf_reset(&author);
		strbuf_addf(&author, "%s <%s>", a->name, a->mail);
		read_end();
	}

	/* timestamp */
	{
		struct tm tm;
		if (read_list()) goto err;
		if (read_strbuf(&time)) goto err;
		if (!strptime(time.buf, "%Y-%m-%dT%H:%M:%S", &tm)) goto err;
		strbuf_addch(&author, ' ');
		strbuf_addf(&author, "%"PRId64" +0000", (int64_t) mktime(&tm));
		read_end();
	}

	/* log message */
	if (have_optional()) {
		if (read_strbuf(&log)) goto err;
		strbuf_complete_line(&log);
		read_end();
	}

	read_end();
	if (verbose) fprintf(stderr, "\n");

	{
		const char* s;
		if ((s = read_word()) == NULL || strcmp(s, "done")) goto err;
		read_success();
	}

	/* now commit */

	for (i = 0; i < refn; i++) {
		unsigned char newcmt[20];
		struct ref_lock* ref_lock = NULL;
		struct svnref* r = refs[i];

		if (!r->dirty) continue;

		if (!r->index.cache_tree)
			r->index.cache_tree = cache_tree();
		if (cache_tree_update(r->index.cache_tree, r->index.cache, r->index.cache_nr, 0))
			die("failed to update cache tree");

		/* for tags we need to see if the tree has been modified
		 * from the previous value and create a dummy commit in
		 * that case */
		if (!r->delete && (!r->istag || hashcmp(r->index.cache_tree->sha1, r->tree))) {
			strbuf_reset(&cmt);
			strbuf_addf(&cmt, "tree %s\n", sha1_to_hex(r->index.cache_tree->sha1));

			if (!is_null_sha1(r->parent)) {
				strbuf_addf(&cmt, "parent %s\n", sha1_to_hex(r->parent));
			}

			strbuf_addf(&cmt, "author %s\n", author.buf);
			strbuf_addf(&cmt, "committer %s\n", author.buf);

			strbuf_addch(&cmt, '\n');
			strbuf_addbuf(&cmt, &log);

			if (write_sha1_file(cmt.buf, cmt.len, "commit", r->parent))
				die("failed to create commit");
		}

		/* Now we create an annotated tag wrapped around either
		 * the commit the tag was branched from or the wrapper.
		 * Where a tag is later updated, we either recreate this
		 * tag with a new time (no tree change) or create a new
		 * dummy commit whose parent is the old dummy.
		 */
		if (r->delete) {
			hashcpy(newcmt, null_sha1);
		} else if (r->istag) {
			strbuf_reset(&cmt);
			strbuf_addf(&cmt, "object %s\n"
					"type commit\n"
					"tag %s\n"
					"tagger %s\n"
					"\n",
					sha1_to_hex(r->parent),
					r->ref.buf + strlen("refs/tags/"),
					author.buf);
			strbuf_addbuf(&cmt, &log);

			if (write_sha1_file(cmt.buf, cmt.len, tag_type, newcmt))
				die("failed to create tag");
		} else {
			hashcpy(newcmt, r->parent);
		}

		/* lock the ref */

		if (!r->delete) {
			ref_lock = lock_ref_sha1(r->ref.buf + strlen("refs/"), r->sha1);
			if (!ref_lock) die("failed to grab ref lock");
		}

		/* update the revdb */

		append_to_revdb(r, rev, newcmt);

		/* update the ref */

		if (r->delete) {
			if (delete_ref(r->ref.buf + strlen("refs/"), r->sha1, 0))
				die("failed to remove %s", r->ref.buf);
		} else {
			if (write_ref_sha1(ref_lock, newcmt, "svn-sync update"))
				die("commit failed");
		}

		hashcpy(r->tree, r->index.cache_tree->sha1);
		hashcpy(r->sha1, newcmt);
		r->dirty = 0;

		fprintf(stderr, "commited %d %s %s\n", rev, r->ref.buf, sha1_to_hex(r->sha1));
	}

	strbuf_release(&author);
	strbuf_release(&cmt);
	strbuf_release(&log);
	strbuf_release(&time);
	strbuf_release(&name);
	fprintf(stderr, "finished commit %d\n", rev);
	return;
err:
	die(_("malformed commit"));
}

static char* clean_path(char* p) {
	char* e;
	if (*p == '/') p++;
	e = p + strlen(p);
	if (e > p && e[-1] == '/') e[-1] = '/';
	return p;
}

static void setup_globals() {
	tmpf = tmpfile();
	freopen(NULL, "wb+", tmpf);

	mkdir(git_path("svn"), 0777);
	mkdir(git_path("svn/heads"), 0777);
	mkdir(git_path("svn/tags"), 0777);

	parse_authors();

	if (trunk) trunk = clean_path(trunk);
	if (branches) branches = clean_path(branches);
	if (tags) tags = clean_path(tags);
}

static void reconnect(const char* repo, const char* user, const char* pass) {
	char pathsep;
	char *host, *port, *path;
	struct addrinfo hints, *res, *ai;
	int err;

	if (strncmp(repo, "svn://", strlen("svn://")) != 0)
		die(_("only svn repositories are supported"));

	if (use_stdin) {
		infd = fileno(stdin);
		outfd = fileno(stdout);
		return;
	}

	if (infd >= 0) close(infd);
	if (outfd >= 0) close(outfd);
	infd = outfd = -1;

	host = (char*) repo + strlen("svn://");

	path = strchr(host, '/');
	if (!path) path = host + strlen(host);
	pathsep = *path;
	*path = '\0';

	port = strchr(host, ':');
	if (port) *port = '\0';

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;

	err = getaddrinfo(host, port ? port : "3690", &hints, &res);
	if (port) *port = '\0';
	*path = pathsep;

	if (err)
		die("failed to connect to %s:%s", host, port ? port : "3690");

	for (ai = res; ai != NULL; ai = ai->ai_next) {
		int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (fd < 0) continue;

		if (connect(fd, ai->ai_addr, ai->ai_addrlen)) {
			close(fd);
			continue;
		}

		infd = outfd = fd;
		break;
	}

	if (infd < 0)
		die("failed to connect to %s", repo);

	/* TODO: client software version and client capabilities */
	sendf("( 2 ( edit-pipeline svndiff1 ) %d:%s )\n( CRAM-MD5 ( ) )\n",
		       	(int) strlen(repo), repo);

	/* TODO: we don't care about capabilities/versions right now */
	if (strcmp(read_response(), "success")) die("server error");

	/* minver then maxver */
	if (read_number() > 2 || read_number() < 2)
		die(_("version mismatch"));

	read_end_response();

	/* TODO: read the mech lists et all */
	read_success();

	if (!user || !pass)
		die("user/pass not specified");

	cram_md5(user, pass);

	sendf("( reparent ( %d:%s ) )\n", (int) strlen(repo), repo);

	read_success(); /* auth */
	read_success(); /* repo info */
	read_success(); /* reparent */
	read_success(); /* reparent again */
}

static int split_revisions(int rev[2]) {
	char *s;
	if (!revisions) return 0;

	rev[0] = strtol(revisions, &s, 10);

	if (rev[0] <= 0 || rev[0] >= INT_MAX) return -1;
	if (!*s) return 1;
	if (*s && *s != ':') return -1;

	rev[1] = strtol(s, &s, 10);
	if (rev[1] <= 0 || rev[1] >= INT_MAX) return -1;
	if (rev[1] < rev[0]) return -1;

	return 2;
}

static int latest_rev(int fd) {
	char buf[256];
	ssize_t len;
	if (lseek(fd, 0, SEEK_SET)) die_errno("seek %s", git_path("svn/latest"));

	len = read(fd, buf, sizeof(buf)-1);
	if (len < 0) die_errno("read %s", git_path("svn/latest"));

	buf[len] = '\0';
	return atoi(buf);
}

#define STR(x) #x

int cmd_svn_fetch(int argc, const char **argv, const char *prefix) {
	struct lock_file lock_file;
	int revfd;
	int64_t n;
	int rev[2];

	argc = parse_options(argc, argv, prefix, builtin_svn_fetch_options,
		       	builtin_svn_fetch_usage, 0);

	if (argc != 1)
		usage_msg_opt(argc > 1 ? _("Too many arguments.") : _("Too few arguments"),
			builtin_svn_fetch_usage, builtin_svn_fetch_options);

	setup_globals();
	reconnect(argv[0], pulluser, pullpass);

	revfd = hold_lock_file_for_append(&lock_file, git_path("svn/latest"), LOCK_DIE_ON_ERROR | LOCK_NODEREF);

	switch (split_revisions(rev)) {
	case 0:
		rev[1] = INT_MAX;
		rev[0] = latest_rev(revfd) + 1;
		break;
	case 1:
		rev[1] = rev[0];
		rev[0] = latest_rev(revfd) + 1;
		break;
	case 2:
		break;
	default:
		die("invalid revision range");
	}

nextpoll:
	sendf("( get-latest-rev ( ) )\n");

	read_success(); /* latest rev */
	read_response(); /* latest rev again */
	n = read_number();
	if (n < 0 || n > INT_MAX) die("latest-rev failed");
	rev[1] = min(rev[1], (int) n);
	read_end_response();

	fprintf(stderr, "rev %d %d\n", rev[0], rev[1]);
	while (rev[0] <= rev[1]) {
		char buf[sizeof(STR(INT_MAX))+1];
		get_commit(rev[0]);

		snprintf(buf, sizeof(buf), "%d\n", rev[0]);

		if (lseek(revfd, 0, SEEK_SET) < 0
			|| ftruncate(revfd, 0)
			|| write(revfd, buf, strlen(buf)) != strlen(buf)) {
			die_errno("write %s", git_path("svn/latest"));
		}

		rev[0]++;
	}

	if (interval > 0) {
		sleep(interval);
		goto nextpoll;
	} else if (interval == 0) {
		select(0, NULL, NULL, NULL, NULL);
		goto nextpoll;
	}

	if (commit_lock_file(&lock_file))
		die("failed to commit latest lock");

	return 0;
}

static const char* dtoken(int dir) {
	static int bufnum;
	static char bufs[4][32];
	char* buf1 = bufs[bufnum++ & 3];
	char* buf2 = bufs[bufnum++ & 3];
	sprintf(buf1, "d%d", dir);
	sprintf(buf2, "%d:%s", (int) strlen(buf1), buf1);
	return buf2;
}

static int fcount;
static struct svnref* curref;

static const char* ftoken() {
	static char buf[32];
	sprintf(buf, "c%d", ++fcount);
	sprintf(buf, "%d:c%d", (int) strlen(buf), fcount);
	return buf;
}

/* check that no commits have been inserted on our branch between r->rev
 * and rev */
static void check_for_svn_commits(struct svnref* r, int rev) {
	const char* s;

	if (rev <= r->rev + 1) {
		return;
	}

	sendf("( log ( ( %d:%s ) " /* (path...) */
			"( %d ) ( %d ) " /* start/end revno */
			"false false " /* changed-paths strict-node */
			"0 false " /* limit include-merged-revisions */
			"revprops ( ) ) )\n",
		(int) r->svn.len,
		r->svn.buf,
		r->rev + 1,
		rev - 1);

	read_success();
	if (!read_list()) {
		die("commits inserted during push");
	}

	if ((s = read_word()) == NULL || strcmp(s, "done"))
		die("log failed");
	if (verbose) fputc('\n', stderr);

	read_success();
}

static size_t common_directory(const char* a, const char* b, int* depth) {
	int off;
	const char* ab = a;

	off = 0;
	while (*a && *b && *a == *b) {
		if (*a == '/') {
			(*depth)++;
			off = a + 1 - ab;
		}
		a++;
		b++;
	}

	return off;
}

static struct strbuf cpath = STRBUF_INIT;
static int cdepth;

static int change_dir(const char* path) {
	const char *p, *d;
	int off, depth;

	off = common_directory(path, cpath.buf, &depth);

	/* cd .. to the common root */
	while (cdepth > depth) {
		sendf("( close-dir ( %s ) )\n", dtoken(cdepth));
		cdepth--;
	}

	strbuf_setlen(&cpath, off);

	/* cd down to the new path */
	d = p = path + off;
	for (;;) {
		char* d = strchr(p, '/');
		if (!d) break;

		sendf("( open-dir ( %d:%.*s %s %s ( ) ) )\n",
			(int) (d - path), (int) (d - path), path,
			dtoken(cdepth),
			dtoken(cdepth+1));

		/* include the / at the end */
		d++;
		strbuf_add(&cpath, p, d - p);
		p = d;
		cdepth++;
	}

	return cdepth;
}

static void dir_changed(int dir, const char* path) {
	strbuf_reset(&cpath);
	strbuf_addstr(&cpath, path);
	if (*path) strbuf_addch(&cpath, '/');
	cdepth = dir;
}

static void send_delta_chunk(const char* tok, const void* data, size_t sz) {
	sendf("( textdelta-chunk ( %s %d:", tok, (int) sz);
	sendb(data, sz);
	sendf(" ) )\n");
}

static void change(struct diff_options* op,
	       	unsigned omode,
	       	unsigned nmode,
		const unsigned char* osha1,
		const unsigned char* nsha1,
		const char* path,
		unsigned odsubmodule,
		unsigned ndsubmodule)
{
	unsigned char ins[MAX_INS_LEN], *inp = ins;
	unsigned char hdr[5*MAX_VARINT_LEN], *hp = hdr;
	enum object_type type;
	const char* tok;
	void* data;
	size_t sz;
	int dir;

	fprintf(stderr, "change mode %x/%x, sha1 %s/%s path %s\n",
			omode, nmode, sha1_to_hex(osha1), sha1_to_hex(nsha1), path);

	/* dont care about changed directories */
	if (!S_ISREG(nmode)) return;

	dir = change_dir(path);

	/* TODO make this actually use diffcore */

	data = read_sha1_file(nsha1, &type, &sz);

	if (type != OBJ_BLOB)
		die("unexpected object type for %s", sha1_to_hex(nsha1));

	inp = encode_instruction(inp, COPY_FROM_NEW, 0, sz);

	hp = encode_varint(hp, 0); /* source off */
	hp = encode_varint(hp, 0); /* source len */
	hp = encode_varint(hp, sz); /* target len */
	hp = encode_varint(hp, inp - ins); /* ins len */
	hp = encode_varint(hp, sz); /* data len */

	tok = ftoken();
	sendf("( open-file ( %d:%s %s %s ( %d ) ) )\n"
		"( apply-textdelta ( %s ( ) ) )\n",
		(int) strlen(path), path, dtoken(dir), tok, curref->rev,
		tok);

	send_delta_chunk(tok, "SVN\0", 4);
	send_delta_chunk(tok, hdr, hp - hdr);
	send_delta_chunk(tok, ins, inp - ins);
	send_delta_chunk(tok, data, sz);

	sendf("( textdelta-end ( %s ) )\n"
		"( close-file ( %s ( ) ) )\n",
		tok, tok);

	diff_change(op, omode, nmode, osha1, nsha1, path, odsubmodule, ndsubmodule);
}

static void addremove(struct diff_options* op,
		int addrm,
		unsigned mode,
		const unsigned char* sha1,
		const char* path,
		unsigned dsubmodule)
{
	static struct strbuf rmdir = STRBUF_INIT;
	static int in_rmdir;
	int dir;
	size_t plen = strlen(path);

	fprintf(stderr, "addremove %d/%c mode %x sha1 %s path %s\n",
			addrm, addrm, mode, sha1_to_hex(sha1), path);

	/* diff recursively returns deleted folders, but svn only needs
	 * the root */
	if (addrm == '-' && in_rmdir && plen >= rmdir.len && !memcmp(path, rmdir.buf, rmdir.len)) {
		return;
	}

	in_rmdir = 0;
	dir = change_dir(path);

	if (addrm == '-' && S_ISDIR(mode)) {
		strbuf_reset(&rmdir);
		strbuf_add(&rmdir, path, plen);
		strbuf_addch(&rmdir, '/');
		in_rmdir = 1;

		sendf("( delete-entry ( %d:%s ( ) %s ) )\n",
			(int) plen, path, dtoken(dir));

	} else if (addrm == '+' && S_ISDIR(mode)) {
		sendf("( add-dir ( %d:%s %s %s ( ) ) )\n",
			(int) plen, path, dtoken(dir), dtoken(dir+1));

		dir_changed(++dir, path);

	} else if (addrm == '-' && S_ISREG(mode)) {
		sendf("( delete-entry ( %d:%s ( ) %s) )\n",
			(int) plen, path, dtoken(dir));

	} else if (addrm == '+' && S_ISREG(mode)) {
		unsigned char ins[MAX_INS_LEN], *inp = ins;
		unsigned char hdr[5*MAX_VARINT_LEN], *hp = hdr;
		enum object_type type;
		const char* tok;
		void* data;
		size_t sz;

		data = read_sha1_file(sha1, &type, &sz);

		if (type != OBJ_BLOB)
			die("unexpected object type for %s", sha1_to_hex(sha1));

		inp = encode_instruction(inp, COPY_FROM_NEW, 0, sz);

		hp = encode_varint(hp, 0); /* source off */
		hp = encode_varint(hp, 0); /* source len */
		hp = encode_varint(hp, sz); /* target len */
		hp = encode_varint(hp, inp - ins); /* ins len */
		hp = encode_varint(hp, sz); /* data len */

		/* TOOD: find copies */

		tok = ftoken();
		sendf("( add-file ( %d:%s %s %s ( ) ) )\n"
			"( apply-textdelta ( %s ( ) ) )\n",
			(int) strlen(path), path, dtoken(dir), tok,
			tok);

		send_delta_chunk(tok, "SVN\0", 4);
		send_delta_chunk(tok, hdr, hp - hdr);
		send_delta_chunk(tok, ins, inp - ins);
		send_delta_chunk(tok, data, sz);

		sendf("( textdelta-end ( %s ) )\n"
			"( close-file ( %s ( ) ) )\n",
			tok, tok);
	}

	diff_addremove(op, addrm, mode, sha1, path, dsubmodule);
}

static void output(struct diff_queue_struct *q,
		struct diff_options* op,
		void* data)
{
	int i;
	fprintf(stderr, "output %d %p\n", q->nr, data);
	for (i = 0; i < q->nr; i++) {
		struct diff_filepair* p = q->queue[i];
		fprintf(stderr, "output %s %s\n", p->one->path, p->two->path);
	}
}

/* returns the rev number */
static int send_commit(struct svnref* r, struct commit* parent, struct commit* cmt) {
	struct diff_options op;
	int dir;
	const char* msg;
	int64_t n;

	fcount = 0;
	curref = r;

	parse_commit(cmt);
	find_commit_subject(cmt->buffer, &msg);

	sendf("( commit ( %d:%s ) )\n"
		"( open-root ( ( ) %s ) )\n",
		(int) strlen(msg), msg,
		dtoken(0));

	read_success();
	read_success();

	dir = change_dir(r->svn.buf);

	sendf("( %s-dir ( %d:%s %s %s ( ) ) )\n",
		r->create ? "add" : "open",
		(int) r->svn.len,
		r->svn.buf,
		dtoken(dir),
		dtoken(dir+1));

	dir_changed(++dir, r->svn.buf);

	diff_setup(&op);
	op.output_format = DIFF_FORMAT_CALLBACK;
	op.change = &change;
	op.add_remove = &addremove;
	op.format_callback = &output;
	DIFF_OPT_SET(&op, RECURSIVE);
	DIFF_OPT_SET(&op, IGNORE_SUBMODULES);
	DIFF_OPT_SET(&op, TREE_IN_RECURSIVE);

	if (r->svn.len)
		strbuf_addch(&r->svn, '/');

	fprintf(stderr, "diff %s to %s\n", sha1_to_hex(parent->object.sha1), sha1_to_hex(cmt->object.sha1));
	if (diff_tree_sha1(parent->object.sha1, cmt->object.sha1, r->svn.buf, &op))
		die("diff tree failed");

	if (r->svn.len)
		strbuf_setlen(&r->svn, r->svn.len - 1);

	change_dir("");
	sendf("( close-dir ( %s ) )\n"
		"( close-edit ( ) )\n",
		dtoken(0));

	read_success();
	read_success();

	/* commit-info */
	if (read_list()) goto err;
	n = read_number();
	if (n < 0 || n > INT_MAX) goto err;
	read_end();
	if (verbose) fputc('\n', stderr);

	return (int) n;

err:
	die("commit failed");
}

int cmd_svn_push(int argc, const char **argv, const char *prefix) {
	struct commit *from, *to, *cmt;
	struct author* preva = NULL;
	struct strbuf buf = STRBUF_INIT;
	struct rev_info rev;
	struct svnref* r;

	argc = parse_options(argc, argv, prefix, builtin_svn_push_options,
		       	builtin_svn_push_usage, 0);

	if (argc != 4)
		usage_msg_opt(argc > 4 ? _("Too many arguments.") : _("Too few arguments"),
			builtin_svn_push_usage, builtin_svn_push_options);

	setup_globals();

	if (!strcmp(argv[1], "refs/heads/master")) {
		strbuf_addstr(&buf, trunk);

	} else if (!strncmp(argv[1], "refs/heads/", strlen("refs/heads/"))) {
		if (!branches)
			die("in order to push a branch --branches must be specified");
		strbuf_addstr(&buf, branches);
		strbuf_addstr(&buf, argv[1] + strlen("refs/heads"));

	} else if (!strncmp(argv[1], "refs/tags/", strlen("refs/tags/"))) {
		if (!tags)
			die("in order to push a tag --tags must be specified");
		strbuf_addstr(&buf, tags);
		strbuf_addstr(&buf, argv[1] + strlen("refs/tags"));

	} else {
		die("invalid ref specified, must be of the form refs/heads/x or refs/tags/x");
	}

	from = lookup_commit_reference_by_name(argv[2]);
	to = lookup_commit_reference_by_name(argv[3]);

	if (!from) die("unknown commit %s", argv[2]);
	if (!to) die("unknown commit %s", argv[3]);

	fprintf(stderr, "from %s to %s\n", sha1_to_hex(from->object.sha1), sha1_to_hex(to->object.sha1));

	if (!hashcmp(from->object.sha1, null_sha1) && !hashcmp(to->object.sha1, null_sha1))
		return 0;

	r = find_ref(&buf, hashcmp(from->object.sha1, null_sha1) ? 0 : REF_MAY_BE_NEW);

	if (hashcmp(r->sha1, from->object.sha1))
		die("base given is not the current ref value");

	/* need to delete the branch */
	if (is_null_sha1(to->object.sha1)) {
		int dir;

		preva = get_commit_author(from);
		reconnect(argv[0], preva->user, preva->pass);

		sendf("( commit %d:Remove %s )\n"
			"( open-root ( ( ) %s ) )\n",
			(int) (r->svn.len + strlen("Remove ")), r->svn.buf,
			dtoken(0));

		dir = change_dir(r->svn.buf);
		sendf("( delete-entry ( %d:%s ( ) %s ) )\n",
			(int) r->svn.len, r->svn.buf, dtoken(dir));

		change_dir("");
		sendf("( close-dir ( %s ) )\n"
			"( close-edit ( ) )\n",
			dtoken(0));

		read_success();
		return 0;
	}

	init_revisions(&rev, NULL);
	add_pending_object(&rev, &to->object, argv[3]);
	rev.first_parent_only = 1;
	rev.reverse = 1;

	if (!is_null_sha1(from->object.sha1)) {
		from->object.flags |= UNINTERESTING;
		add_pending_object(&rev, &from->object, argv[2]);
	}

	if (prepare_revision_walk(&rev))
		die("prepare rev walk failed");

	while ((cmt = get_revision(&rev)) != NULL) {
		int svnrev;
		struct author* a;

		fprintf(stderr, "commit %s\n", sha1_to_hex(cmt->object.sha1));

		a = get_commit_author(cmt);
		if (a != preva) {
			reconnect(argv[0], a->user, a->pass);
			preva = a;
		}

		svnrev = send_commit(r, from, cmt);
		check_for_svn_commits(r, svnrev);
		append_to_revdb(r, svnrev, cmt->object.sha1);
		r->rev = svnrev;
		r->create = 0;
		from = cmt;
	}

	return 0;
}
