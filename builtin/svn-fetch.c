#include "git-compat-util.h"
#include "parse-options.h"
#include "gettext.h"
#include "cache.h"
#include "cache-tree.h"
#include "refs.h"
#include "unpack-trees.h"
#include "commit.h"
#include "tag.h"
#include "diff.h"
#include "revision.h"
#include "diffcore.h"
#include "run-command.h"

#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static const char *svnuser;
static const char *trunk;
static const char *branches;
static const char *tags;
static const char *remotedir;
static const char *trunkref = "master";
static int last_revision = INT_MAX;
static int verbose;
static int pre_receive;
static int svnfdc = 1;
static int svnfd;
static const char* url;
static enum eol svn_eol = EOL_UNSET;

#define FETCH_AT_ONCE 1000

static const char* const builtin_svn_fetch_usage[] = {
	"git svn-fetch [options]",
	NULL,
};

static struct option builtin_svn_fetch_options[] = {
	OPT_STRING(0, "user", &svnuser, "user", "svn username"),
	OPT_BOOLEAN('v', "verbose", &verbose, "verbose logging of all svn traffic"),
	OPT_INTEGER('r', "revision", &last_revision, "revisions to fetch up to"),
	OPT_INTEGER('c', "connections", &svnfdc, "number of concurrent connections"),
	OPT_END()
};

static const char* const builtin_svn_push_usage[] = {
	"git svn-push [options] <ref> <from commit> <to commit>",
	"git svn-push [options] --pre-receive",
	NULL,
};

static struct option builtin_svn_push_options[] = {
	OPT_STRING(0, "user", &svnuser, "user", "default svn username"),
	OPT_BOOLEAN('v', "verbose", &verbose, "verbose logging of all svn traffic"),
	OPT_BOOLEAN(0, "pre-receive", &pre_receive, "run as a pre-receive hook"),
	OPT_END()
};

static const char* const builtin_svn_merge_base_usage[] = {
	"git svn-merge-base <commitish>",
	NULL,
};

static struct option builtin_svn_merge_base_options[] = {
	OPT_END()
};

static int config(const char *var, const char *value, void *dummy) {
	if (!strcmp(var, "svn.trunk")) {
		return git_config_string(&trunk, var, value);
	}
	if (!strcmp(var, "svn.branches")) {
		return git_config_string(&branches, var, value);
	}
	if (!strcmp(var, "svn.tags")) {
		return git_config_string(&tags, var, value);
	}
	if (!strcmp(var, "svn.user")) {
		return git_config_string(&svnuser, var, value);
	}
	if (!strcmp(var, "svn.url")) {
		return git_config_string(&url, var, value);
	}
	if (!strcmp(var, "svn.remote")) {
		return git_config_string(&remotedir, var, value);
	}
	if (!strcmp(var, "svn.trunkref")) {
		return git_config_string(&trunkref, var, value);
	}
	if (!strcmp(var, "svn.eol")) {
		if (value && !strcasecmp(value, "lf"))
			svn_eol = EOL_LF;
		else if (value && !strcasecmp(value, "crlf"))
			svn_eol = EOL_CRLF;
		else if (value && !strcasecmp(value, "native"))
			svn_eol = EOL_NATIVE;
		else
			svn_eol = EOL_UNSET;
		return 0;
	}
	return git_default_config(var, value, dummy);
}

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) ((a) < (b) ? (b) : (a))
#endif

struct inbuffer {
	char buf[4096];
	int b, e;
};
static struct inbuffer* inbuf;

static int readc() {
	if (inbuf->b == inbuf->e) {
		inbuf->b = 0;
		inbuf->e = xread(svnfd, inbuf->buf, sizeof(inbuf->buf));
		if (inbuf->e <= 0) return EOF;
	}

	return inbuf->buf[inbuf->b++];
}

static void unreadc() {
	inbuf->b--;
}

static int readsvn(void* u, void* p, int n) {
	/* big reads we may as well read directly into the target */
	if (inbuf->e == inbuf->b && n >= sizeof(inbuf->buf) / 2) {
		return xread(svnfd, p, n);

	} else if (inbuf->e == inbuf->b) {
		inbuf->b = 0;
		inbuf->e = xread(svnfd, inbuf->buf, sizeof(inbuf->buf));
		if (inbuf->e <= 0) return inbuf->e;
	}

	n = min(n, inbuf->e - inbuf->b);
	memcpy(p, inbuf->buf + inbuf->b, n);
	inbuf->b += n;
	return n;
}

static int writef(void* u, const void* p, int n) {
	return fwrite(p, 1, n, (FILE*) u);
}

typedef int (*reader)(void*, void*, int);
typedef int (*writer)(void*, const void*, int);

static int writes(void* u, const void* p, int n) {
	strbuf_add((struct strbuf*) u, p, n);
	return n;
}

static int reads(void* u, void* p, int n) {
	struct strbuf* s = u;
	n = min(n, s->len);
	memcpy(p, s->buf, n);
	strbuf_remove(s, 0, n);
	return n;
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

static int get_md5_hex(const char *hex, unsigned char *sha1)
{
	int i;
	for (i = 0; i < 16; i++) {
		unsigned int val;
		/*
		 * hex[1]=='\0' is caught when val is checked below,
		 * but if hex[0] is NUL we have to avoid reading
		 * past the end of the string:
		 */
		if (!hex[0])
			return -1;
		val = (hexval(hex[0]) << 4) | hexval(hex[1]);
		if (val & ~0xff)
			return -1;
		*sha1++ = val;
		hex += 2;
	}
	return 0;
}

static const char* md5_to_hex(const unsigned char* md5) {
	static int bufno;
	static char hexbuffer[4][50];
	char *buffer = hexbuffer[3 & ++bufno], *buf = buffer;
	int i;

	for (i = 0; i < 16; i++) {
		unsigned int val = *md5++;
		*buf++ = hex[val >> 4];
		*buf++ = hex[val & 0xf];
	}
	*buf = '\0';

	return buffer;
}

static void copyn(writer wf, void* wd, reader rf, void *rd, int64_t n) {
	while (n > 0) {
		char buf[BUFSIZ];
		int r = rf(rd, buf, min(sizeof(buf), n));
		if (r <= 0)
			die_errno("unexpected end");

		if (wf && wf(wd, buf, r) != r)
			die_errno("failed write");

		n -= r;
	}
}

static void readfull(void *p, reader rf, void* rd, int n) {
	while (n > 0) {
		int r = rf(rd, p, n);
		if (r <= 0)
			die_errno("unexpected end");

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

	if (write_in_full(svnfd, out.buf, out.len) != out.len) {
		die_errno("write");
	}
}

static void sendb(const void* data, size_t sz) {
	if (verbose) {
		print_ascii(&writef, stderr, data, sz);
	}
	if (write_in_full(svnfd, data, sz) != sz) {
		die_errno("write");
	}
}

struct unzip {
	z_stream z;
	reader rf;
	void* rd;
	int rn;
	int flush;
	int outoff;
	unsigned char out[4096];
	unsigned char in[4096];
};

static int readz(void* u, void* p, int n) {
	struct unzip* z = u;
	int r;

	if (!n) return 0;

	if (z->outoff == sizeof(z->out) - z->z.avail_out) {
		z->outoff = 0;
	}

	if (z->outoff) {
		n = min(n, sizeof(z->out) - z->z.avail_out - z->outoff);
		memcpy(p, z->out + z->outoff, n);
		z->outoff += n;
		return n;
	}

	if (n < sizeof(z->out)) {
		z->z.avail_out = sizeof(z->out);
		z->z.next_out = z->out;
	} else {
		z->z.avail_out = n;
		z->z.next_out = p;
	}

	while (max(n, sizeof(z->out)) == z->z.avail_out) {
		if (z->rn && !z->z.avail_in) {
			readfull(z->in, z->rf, z->rd, min(z->rn, sizeof(z->in)));
			z->z.avail_in = min(z->rn, sizeof(z->in));
			z->z.next_in = z->in;
			z->rn -= z->z.avail_in;
		}

		r = inflate(&z->z, z->flush);

		if (r == Z_STREAM_END && !z->rn) {
			z->flush = Z_FINISH;
		} else if (r != Z_OK) {
			return -1;
		}
	}

	if (n < sizeof(z->out)) {
		n = min(n, sizeof(z->out) - z->z.avail_out);
		memcpy(p, z->out, n);
		z->outoff = n;
		return n;
	} else {
		return n - z->z.avail_out;
	}
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

static void apply_svndiff_win(struct strbuf* tgt, const void* src, size_t srcsz, reader df, void* dd, int ver) {
	struct unzip z;
	struct strbuf ins = STRBUF_INIT;
	int64_t srco = read_varint(df, dd);
	int64_t srcl = read_varint(df, dd);
	int64_t tgtl = read_varint(df, dd);
	int64_t insc = read_varint(df, dd);
	int64_t datac = read_varint(df, dd);
	int64_t insl = insc;
	int64_t datal = datac;
	int64_t w = 0;

	if (srco + srcl > srcsz) goto err;

	if (ver > 0) {
		unsigned char buf[MAX_VARINT_LEN];
		insl = read_varint(df, dd);
		insc -= encode_varint(buf, insl) - buf;
	}

	if (insc < insl) {
		memset(&z, 0, sizeof(z));
		inflateInit(&z.z);
		z.rf = df;
		z.rd = dd;
		z.rn = insc;
		copyn(&writes, &ins, &readz, &z, insl);
		inflateEnd(&z.z);
	} else {
		copyn(&writes, &ins, df, dd, insl);
	}

	if (ver > 0) {
		unsigned char buf[MAX_VARINT_LEN];
		datal = read_varint(df, dd);
		datac -= encode_varint(buf, datal) - buf;
	}

	if (datac < datal) {
		memset(&z, 0, sizeof(z));
		inflateInit(&z.z);
		z.rf = df;
		z.rd = dd;
		z.rn = datac;
		df = &readz;
		dd = &z;
	}

	while (ins.len) {
		int64_t off, len;
		int tgtr;
		switch (read_instruction(&reads, &ins, &off, &len)) {
		case COPY_FROM_SOURCE:
			if (off + len > srcl) goto err;
			strbuf_add(tgt, (char*) src + srco + off, len);
			w += len;
			break;

		case COPY_FROM_TARGET:
			tgtr = min(w - off, len);
			if (tgtr <= 0) goto err;

			off = tgt->len - w + off;

			/* len may be greater than tgtr. In this case we
			 * just repeat [tgto,tgto+tgtr]
			 */
			while (len) {
				int n = min(len, tgtr);
				strbuf_add(tgt, tgt->buf + off, n);
				len -= n;
				w += n;
			}
			break;

		case COPY_FROM_NEW:
			if (len > datal) goto err;
			copyn(&writes, tgt, df, dd, len);
			w += len;
			datal -= len;
			break;

		default:
			goto err;
		}
	}

	if (dd == &z) {
		inflateEnd(&z.z);
		copyn(NULL, NULL, z.rf, z.rd, z.rn);
	}

	if (w != tgtl || datal) goto err;

	return;
err:
	die("invalid svndiff");
}

static void apply_svndiff(struct strbuf* tgt, const void* src, size_t srcsz, reader df, void* dd, int (*eof)(void*)) {
	unsigned char hdr[4];
	readfull(hdr, df, dd, 4);
	if (memcmp(hdr, "SVN", 3))
		goto err;

	if (hdr[3] > 1)
		goto err;

	while (!eof(dd)) {
		apply_svndiff_win(tgt, src, srcsz, df, dd, hdr[3]);
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

static int skip_string() {
	int64_t n = read_string_size();
	if (n < 0) return -1;
	copyn(verbose ? &writef : NULL, stderr, &readsvn, NULL, n);
	return n;
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

static void read_done() {
	const char* s = read_word();
	if (!s || strcmp(s, "done"))
		die("unexpected failure");
	if (verbose) fputc('\n', stderr);
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
	struct strbuf hex = STRBUF_INIT;
	int64_t sz;
	HMAC_CTX hmac;

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

	print_hex(&writes, &hex, hash, sizeof(hash));
	sendf("%d:%s %s\n", (int) (strlen(user) + 1 + hex.len), user, hex.buf);

	strbuf_release(&hex);
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
		if (skip_string() < 0) goto err;

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

	if (verbose) print_hex(&writef, stderr, p, r);

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
	if (!prefixcmp(name->buf, "../")) goto err;
	if (!prefixcmp(name->buf, "./")) goto err;
	if (strstr(name->buf, "/../")) goto err;
	if (strstr(name->buf, "/./")) goto err;
	if (!suffixcmp(name->buf, "/..")) goto err;
	if (!suffixcmp(name->buf, "/.")) goto err;

	return;
err:
	die("invalid path name %s", name->buf);
}

static const char* cmt_to_hex(struct commit* c) {
	return sha1_to_hex(c ? c->object.sha1 : null_sha1);
}

static const unsigned char* cmt_sha1(struct commit* c) {
	return c ? c->object.sha1 : null_sha1;
}

static int parse_svnrev(struct commit* c) {
	char* p = strstr(c->buffer, "\nrevision ");
	if (!p) die("invalid svn commit %s", cmt_to_hex(c));
	p += strlen("\nrevision ");
	return atoi(p);
}

static void parse_svnpath(struct commit* c, struct strbuf* buf) {
	char* e;
	char* p = strstr(c->buffer, "\npath ");
	if (!p) die("invalid svn commit %s", cmt_to_hex(c));
	p += strlen("\npath ");
	e = strchr(p, '\n');
	if (!e) die("invalid svn commit %s", cmt_to_hex(c));
	strbuf_add(buf, p, e-p);
}

static struct commit* svn_commit(struct commit* c) {
	if (!c->parents || !c->parents->item)
		die("invalid svn commit %s", cmt_to_hex(c));
	/* In the case of no git commit, but we have a previous svn
	 * commit, the svn parent is repeated twice. That way we can
	 * distinguish that case from a git commit but no svn commit */
	if (c->parents->next && c->parents->item == c->parents->next->item) {
		return NULL;
	}
	return c->parents->item;
}

static struct commit* svn_parent(struct commit* c) {
	if (!c->parents)
		die("invalid svn commit %s", cmt_to_hex(c));
	return c->parents->next ? c->parents->next->item : NULL;
}

static int cmp_cache_name_compare(const void *a_, const void *b_) {
	const struct cache_entry *ce1, *ce2;
	int r;

	ce1 = *((const struct cache_entry **)a_);
	ce2 = *((const struct cache_entry **)b_);
	if ((ce1->ce_flags & CE_STAGEMASK) || (ce2->ce_flags & CE_STAGEMASK))
		die("have a stage");
	r = cache_name_compare(ce1->name, ce1->ce_flags,
				  ce2->name, ce2->ce_flags);
	if (!r)
		die("have a dup");
	return r;
}

static int checkout_tree_copy(struct index_state* idx, struct tree* tree, struct strbuf* base) {
	struct tree_desc desc;
	struct name_entry entry;
	size_t baselen = base->len;

	if (parse_tree(tree)) {
		return -1;
	}

	init_tree_desc(&desc, tree->buffer, tree->size);

	while (tree_entry(&desc, &entry)) {
		strbuf_setlen(base, baselen);
		if (baselen) strbuf_addch(base, '/');
		strbuf_addstr(base, entry.path);

		if (S_ISDIR(entry.mode)) {
			struct tree* dir = lookup_tree(entry.sha1);
			if (parse_tree(dir))
			       	return -1;
			if (checkout_tree_copy(idx, dir, base))
				return -1;
		} else {
			struct cache_entry* ce = make_cache_entry(entry.mode, entry.sha1, base->buf, 0, 0);
			if (index_name_pos(idx, base->buf, base->len) >= 0)
				die("name already exists %s", base->buf);
			add_index_entry(idx, ce, ADD_CACHE_JUST_APPEND);
		}
	}

	return 0;
}

static int checkout_tree_search(struct index_state* idx, unsigned char* blob, struct tree* tree, const char* from, const char* to) {
	struct tree_desc desc;
	struct name_entry entry;
	const char* slash;

	if (!*from) {
		int ret;
		struct strbuf buf = STRBUF_INIT;
		strbuf_addstr(&buf, to);
		ret = checkout_tree_copy(idx, tree, &buf);
		strbuf_release(&buf);
		return ret;
	}

	if (parse_tree(tree)) {
		return -1;
	}

	slash = strchr(from, '/');
	if (!slash) slash = from + strlen(from);

	init_tree_desc(&desc, tree->buffer, tree->size);

	while (tree_entry(&desc, &entry)) {
		size_t pathlen = strlen(entry.path);

		if (pathlen != slash - from) continue;
		if (memcmp(entry.path, from, pathlen)) continue;

		if (S_ISDIR(entry.mode)) {
			return checkout_tree_search(idx, blob, lookup_tree(entry.sha1), *slash ? slash+1 : slash, to);

		} else if (!S_ISDIR(entry.mode) && *slash == '\0') {
			if (!blob) return -1;
			hashcpy(blob, entry.sha1);
			return 0;
		}
	}

	return 0;
}

static int checkout_tree(struct index_state* idx, struct tree* tree, const char* from, const char* to) {
	size_t cachenr = idx->cache_nr;
	int ins;

	ins = index_name_pos(idx, to, strlen(to));
	if (ins >= 0) return -1;
	ins = -ins-1;

	if (checkout_tree_search(idx, NULL, tree, from, to))
		return -1;

	qsort(idx->cache + cachenr, idx->cache_nr - cachenr, sizeof(idx->cache[0]), &cmp_cache_name_compare);

	if (cachenr) {
		int i;
		for (i = 0; i < idx->cache_nr - cachenr; i++) {
			struct cache_entry* ce = idx->cache[ins + i];
			idx->cache[ins + i] = idx->cache[cachenr + i];
			idx->cache[cachenr + i] = ce;
		}
	}

	cache_tree_invalidate_path(idx->cache_tree, to);

	return 0;
}

static int find_file(struct tree* tree, const char* path, unsigned char* blob) {
	return checkout_tree_search(NULL, blob, tree, path, "");
}

struct svnref {
	struct strbuf svn; /* svn root */
	struct strbuf ref; /* svn ref path */
	struct strbuf remote; /* remote ref path */
	struct index_state svn_index;
	struct index_state git_index;

	unsigned int delete : 1;
	unsigned int istag : 1;

	struct commit* iter;
	struct commit* svncmt; /* current value of svn ref */
	struct object* gitobj; /* current value of remote ref, may be tag or commit */
	struct commit* parent; /* parent git commit */
};

static struct svnref** refs;
static size_t refn, refalloc;

static int is_in_dir(char* file, const char* dir, char** rel) {
	size_t sz = strlen(dir);
	if (strncmp(file, dir, sz)) return 0;
	if (file[sz] && file[sz] != '/') return 0;
	if (rel) *rel = file[sz] ? &file[sz+1] : &file[sz];
	return 1;
}

#define TRUNK_REF 0
#define BRANCH_REF 1
#define TAG_REF 2

static void add_refname(struct strbuf* buf, const char* name) {
	while (*name) {
		int ch = *(name++);
		if (ch <= ' '
			|| ch == 0x7F
			|| ch == '~'
			|| ch == '^'
			|| ch == ':'
			|| ch == '\\'
			|| ch == '*'
			|| ch == '?'
			|| ch == '[') {
			strbuf_addch(buf, '_');
		} else {
			strbuf_addch(buf, ch);
		}
	}
}

static void checkout_svncmt(struct svnref* r, struct commit* svncmt) {
	struct commit* gitcmt;

	/* R (replace) log entries may already have content that
	 * we need to clear first */
	discard_index(&r->svn_index);
	discard_index(&r->git_index);

	/* Note r->svncmt may not equal c if this creates a branch or
	 * replaces an existing one. c will be the new source whereas
	 * svncmt will continue to point to the old svn commit */

	if (svncmt) {
		if (parse_commit(svncmt))
			die("invalid object %s", cmt_to_hex(svncmt));

		if (checkout_tree(&r->svn_index, svncmt->tree, "", ""))
			die("failed to checkout %s", cmt_to_hex(svncmt));
	}

	gitcmt = svncmt ? svn_commit(svncmt) : NULL;

	if (gitcmt) {
		if (parse_commit(gitcmt))
			die("invalid object %s", cmt_to_hex(gitcmt));

		if (checkout_tree(&r->git_index, gitcmt->tree, "", ""))
			die("failed to checkout %s", cmt_to_hex(gitcmt));
	}

	r->parent = gitcmt;
}

static struct svnref* create_ref(int type, const char* name) {
	struct svnref* r = NULL;
	unsigned char sha1[20];

	switch (type) {
	case TRUNK_REF:
		r = xcalloc(1, sizeof(*r));
		strbuf_addstr(&r->svn, trunk ? trunk : "");
		strbuf_addstr(&r->ref, "refs/svn/heads/trunk");
		strbuf_addstr(&r->remote, remotedir);
		strbuf_addstr(&r->remote, trunkref);
		break;

	case BRANCH_REF:
		r = xcalloc(1, sizeof(*r));

		strbuf_addstr(&r->svn, branches);
		strbuf_addch(&r->svn, '/');
		strbuf_addstr(&r->svn, name);

		strbuf_addstr(&r->ref, "refs/svn/heads/");
		add_refname(&r->ref, name);

		strbuf_addstr(&r->remote, remotedir);
		add_refname(&r->remote, name);
		break;

	case TAG_REF:
		r = xcalloc(1, sizeof(*r));
		r->istag = 1;

		strbuf_addstr(&r->svn, tags);
		strbuf_addch(&r->svn, '/');
		strbuf_addstr(&r->svn, name);

		strbuf_addstr(&r->ref, "refs/svn/tags/");
		add_refname(&r->ref, name);

		strbuf_addstr(&r->remote, "refs/tags/");
		add_refname(&r->remote, name);
		break;
	}

	if (!read_ref(r->remote.buf, sha1) && !is_null_sha1(sha1)) {
		r->gitobj = parse_object(sha1);
		if (!r->gitobj)
			die("invalid ref %s", name);
	}

	if (!read_ref(r->ref.buf, sha1) && !is_null_sha1(sha1)) {
		r->svncmt = lookup_commit(sha1);
		if (parse_commit(r->svncmt))
			die("invalid ref %s", name);
		checkout_svncmt(r, r->svncmt);
	}

	fprintf(stderr, "\ncreated ref %d %s %s %s %s\n",
		       	(int) refn,
		       	r->ref.buf,
			cmt_to_hex(r->svncmt),
			sha1_to_hex(r->gitobj ? r->gitobj->sha1 : null_sha1),
			cmt_to_hex(r->parent));

	ALLOC_GROW(refs, refn + 1, refalloc);
	refs[refn++] = r;
	return r;
}

static struct svnref* find_svnref_by_path(struct strbuf* name) {
	int i;
	struct svnref* r;
	char *a, *b, *c, *d;

	if (!trunk && !branches && !tags && refn) {
		return refs[0];
	}

	for (i = 0; i < refn; i++) {
		r = refs[i];
		if (prefixcmp(name->buf, r->svn.buf)) {
			continue;
		}

		switch (name->buf[r->svn.len]) {
		case '\0':
			strbuf_setlen(name, 0);
			return r;
		case '/':
			strbuf_remove(name, 0, r->svn.len + 1);
			return r;
		}
	}

	/* names are of the form
	 * branches/foo/...
	 * a        b  c   d
	 */
	a = name->buf;
	d = name->buf + name->len;

	if (!trunk && !branches && !tags) {
		return create_ref(TRUNK_REF, "");

	} else if (trunk && is_in_dir(a, trunk, &b)) {
		strbuf_remove(name, 0, b - a);
		return create_ref(TRUNK_REF, "");


	} else if (branches && is_in_dir(a, branches, &b) && *b) {
		c = memchr(b, '/', d - b);
		if (c) {
			*c = '\0';
			r = create_ref(BRANCH_REF, b);
			strbuf_remove(name, 0, c+1 - a);
		} else {
			r = create_ref(BRANCH_REF, b);
			strbuf_reset(name);
		}
		return r;

	} else if (tags && is_in_dir(a, tags, &b) && *b) {
		c = memchr(b, '/', d - b);
		if (c) {
			*c = '\0';
			r = create_ref(TAG_REF, b);
			strbuf_remove(name, 0, c+1 - a);
		} else {
			r = create_ref(TAG_REF, b);
			strbuf_reset(name);
		}
		return r;

	} else {
		return NULL;
	}
}

static struct svnref* find_svnref_by_refname(const char* name) {
	int i;
	char* real_ref = NULL;
	unsigned char sha1[20];
	int refcount = dwim_ref(name, strlen(name), sha1, &real_ref);
	fprintf(stderr, "%d %s %s %s\n", refcount, name, real_ref, trunkref);

	if (refcount > 1) {
		die("ambiguous ref '%s'", name);
	} else if (!refcount) {
		die("can not find ref '%s'", name);
	}

	for (i = 0; i < refn; i++) {
		struct svnref* r = refs[i];
		if (!strcmp(r->ref.buf, real_ref)) {
			return r;
		}
	}

	if (!prefixcmp(real_ref, "refs/heads/")) {
		if (!strcmp(real_ref + strlen("refs/heads/"), trunkref)) {
			return create_ref(TRUNK_REF, "");
		} else if (!branches) {
			die("in order to push a branch, svn.branches must be set");
		} else {
			return create_ref(BRANCH_REF, real_ref + strlen("refs/heads/"));
		}

	} else if (!prefixcmp(real_ref, "refs/tags/")) {
		if (!tags)
			die("in order to push a tag, svn.tags must be set");

		return create_ref(TAG_REF, real_ref + strlen("refs/tags/"));

	} else {
		die("ref '%s' not a local branch/tag", real_ref);
		return NULL;
	}
}

static struct commit* find_svncmt(struct svnref* r, int rev) {
	struct commit* c = r->svncmt;

	while (c && parse_svnrev(c) > rev) {
		c = svn_parent(c);
		if (c && parse_commit(c)) {
			die("invalid commit %s", cmt_to_hex(c));
		}
	}

	return c;
}

/* reads a path, revision pair */
static struct svnref* read_copy_source(struct strbuf* name, int* rev) {
	int64_t srev;
	struct svnref* sref;

	/* copy-path */
	read_name(name);
	sref = find_svnref_by_path(name);
	if (!sref) return NULL;

	/* copy-rev */
	srev = read_number();
	if (srev < 0 || srev > INT_MAX) goto err;
	*rev = srev;

	return sref;
err:
	die("invalid copy source");
}

static int create_ref_cb(const char* refname, const unsigned char* sha1, int flags, void* cb_data) {
	int i;
	for (i = 0; i < refn; i++) {
		if (!strcmp(refs[i]->ref.buf + strlen("refs/svn/"), refname)) {
			return 0;
		}
	}

	if (!strcmp(refname, "heads/trunk")) {
		create_ref(TRUNK_REF, "");
	} else if (branches && !prefixcmp(refname, "heads/")) {
		create_ref(BRANCH_REF, refname + strlen("heads/"));
	} else if (tags && !prefixcmp(refname, "tags/")) {
		create_ref(TAG_REF, refname + strlen("tags/"));
	}

	return 0;
}

static void add_all_refs() {
	for_each_ref_in("refs/svn/", &create_ref_cb, NULL);
}

// rev is inout
static struct commit* get_next_rev(int *rev) {
	int i;

	while (--(*rev) > 0) {
		for (i = 0; i < refn; i++) {
			struct svnref* r = refs[i];
			int irev = parse_svnrev(r->iter);

			while (r->iter && irev > *rev) {
				r->iter = svn_parent(r->iter);
				if (r->iter && parse_commit(r->iter)) {
					die("invalid commit %s", cmt_to_hex(r->iter));
				}
				irev = r->iter ? parse_svnrev(r->iter) : 0;
			}

			if (r->iter && irev == *rev) {
				return r->iter;
			}
		}
	}

	return NULL;
}

/* Finds the merge base of the commit with all the svn heads. This is
 * done by searching back through the svn commits from the most recent
 * revno back. */
static struct commit* find_copy_source(struct commit* head, int rev) {
	int i;
	struct commit *svn, *res = NULL;
	struct commit *git;
	int srev;

	add_all_refs();

	for (i = 0; i < refn; i++) {
		refs[i]->iter = refs[i]->svncmt;
	}

	srev = rev+1;
	svn = get_next_rev(&srev);
	git = head;

	while (git || svn) {
		if (git && (!svn || git->date > svn->date)) {
			if (git->object.flags & SEEN) {
				res = git->util;
				break;
			}

			git->object.flags |= SEEN;

			if (parse_commit(git))
				die("invalid commit %s", cmt_to_hex(git));

			if (git->parents) {
				git = git->parents->item;
			} else {
				git = NULL;
			}
		} else {
			struct commit *svn2 = svn_commit(svn);

			if (!svn2) {
				res = NULL;
				break;
			}

			if (svn2->object.flags & SEEN) {
				res = svn;
				break;
			}

			svn2->object.flags |= SEEN;
			svn2->util = svn;
			svn = get_next_rev(&srev);
		}
	}

	/* now need to clear the SEEN flags so the next lookup works */

	git = head;
	while (git && (git->object.flags & SEEN)) {
		git->object.flags &= ~SEEN;
		if (!git->parents)
			break;

		git = git->parents->item;
	}

	for (i = 0; i < refn; i++) {
		refs[i]->iter = refs[i]->svncmt;
	}

	srev = rev+1;
	svn = get_next_rev(&srev);
	while (svn != NULL) {
		struct commit* svn2 = svn_commit(svn);

		if (!svn2 || !(svn2->object.flags & SEEN))
			break;

		svn2->object.flags &= ~SEEN;
		svn2->util = NULL;
		svn = get_next_rev(&srev);
	}

	return res;
}

static void read_add_dir(struct svnref* r, int rev) {
	/* path, parent-token, child-token, [copy-path, copy-rev] */

	struct strbuf name = STRBUF_INIT;
	struct strbuf srcname = STRBUF_INIT;
	struct cache_entry* ce;
	char* p;
	size_t dlen;
	int files = 0;
	struct svnref* srcref;
	int srcrev;

	read_name(&name);
	find_svnref_by_path(&name);
	fprintf(stderr, "A %s\n", name.buf);

	/* parent-token, child-token */
	if (skip_string() < 0) goto err;
	if (skip_string() < 0) goto err;

	if (name.len) strbuf_addch(&name, '/');
	dlen = name.len;

	if (have_optional() && (srcref = read_copy_source(&srcname, &srcrev)) != NULL) {
		struct commit* srccmt;
		if (srcrev > rev) goto err;

		srccmt = find_svncmt(srcref, rev);
		if (!srccmt) goto err;

		if (checkout_tree(&r->svn_index, srccmt->tree, srcname.buf, name.buf))
			goto err;

		if (checkout_tree(&r->git_index, svn_commit(srccmt)->tree, srcname.buf, name.buf))
			goto err;

		read_end();
	}

	strbuf_setlen(&name, dlen);

	/* empty folder - add ./.gitempty */
	if (files == 0 && dlen) {
		unsigned char sha1[20];
		if (write_sha1_file(NULL, 0, "blob", sha1))
			die("failed to write .gitempty object");
		strbuf_addstr(&name, ".gitempty");
		ce = make_cache_entry(create_ce_mode(0644), sha1, name.buf, 0, 0);
		add_index_entry(&r->git_index, ce, ADD_CACHE_OK_TO_ADD);
	}

	/* remove ../.gitempty */
	if (dlen) {
		strbuf_setlen(&name, dlen - 1);
		p = strrchr(name.buf, '/');
		if (p) {
			strbuf_setlen(&name, p - name.buf);
			strbuf_addstr(&name, "/.gitempty");
			remove_file_from_index(&r->git_index, name.buf);
		}
	}

	strbuf_release(&srcname);
	strbuf_release(&name);
	return;

err:
	die("malformed update");
}

static void read_add_file(struct svnref* r, int rev, struct strbuf* name, void** srcp, size_t* srcsz) {
	/* name, dir-token, file-token, [copy-path, copy-rev] */
	struct strbuf srcname = STRBUF_INIT;
	struct svnref* srcref;
	int srcrev;
	char* p;

	read_name(name);
	find_svnref_by_path(name);
	fprintf(stderr, "A %s\n", name->buf);

	/* dir-token, file-token */
	if (skip_string() < 0) goto err;
	if (skip_string() < 0) goto err;

	/* see if we have a copy path */
	if (have_optional() && (srcref = read_copy_source(&srcname, &srcrev)) != NULL) {
		unsigned char sha1[20];
		unsigned long srcn;
		enum object_type type;
		struct commit* srccmt;

		if (srcrev > rev) goto err;

		srccmt = find_svncmt(srcref, rev);
		if (!srccmt) goto err;

		if (find_file(srccmt->tree, srcname.buf, sha1))
			goto err;

		*srcp = read_sha1_file(sha1, &type, &srcn);
		if (!srcp || type != OBJ_BLOB) goto err;
		*srcsz = srcn;

		read_end();
	}

	/* remove ./.gitempty */
	p = strrchr(name->buf, '/');
	if (p) {
		struct strbuf empty = STRBUF_INIT;
		strbuf_add(&empty, name->buf, p - name->buf);
		strbuf_addstr(&empty, "/.gitempty");
		remove_file_from_index(&r->git_index, empty.buf);
		strbuf_release(&empty);
	}

	strbuf_release(&srcname);
	return;
err:
	die("malformed update");
}

static void read_open_file(struct svnref* r, int rev, struct strbuf* name, void** srcp, size_t* srcsz) {
	/* name, dir-token, file-token, rev */
	enum object_type type;
	struct cache_entry* ce;
	unsigned long srcn;

	read_name(name);
	find_svnref_by_path(name);
	fprintf(stderr, "M %s\n", name->buf);

	ce = index_name_exists(&r->svn_index, name->buf, name->len, 0);
	if (!ce) goto err;

	*srcp = read_sha1_file(ce->sha1, &type, &srcn);
	if (!srcp || type != OBJ_BLOB) goto err;
	*srcsz = srcn;

	return;
err:
	die("malformed update");
}

static void read_close_file(struct svnref* r, const char* name, const void* data, size_t sz) {
	/* file-token, [text-checksum] */
	struct cache_entry* ce;
	unsigned char sha1[20];
	struct strbuf buf = STRBUF_INIT;

	if (skip_string() < 0) goto err;

	if (write_sha1_file(data, sz, "blob", sha1))
		die_errno("write blob");

	if (have_optional()) {
		unsigned char h1[16], h2[16];
		MD5_CTX ctx;

		strbuf_reset(&buf);
		if (read_strbuf(&buf)) goto err;
		if (get_md5_hex(buf.buf, h1)) goto err;

		MD5_Init(&ctx);
		MD5_Update(&ctx, data, sz);
		MD5_Final(h2, &ctx);

		if (memcmp(h1, h2, sizeof(h1))) {
			ce = index_name_exists(&r->svn_index, name, strlen(name), 0);
			die("hash mismatch for '%s', expected md5 %s, got md5 %s, old sha1 %s, new sha1 %s",
					name,
					md5_to_hex(h2),
					md5_to_hex(h1),
					sha1_to_hex(ce ? ce->sha1 : null_sha1),
					sha1_to_hex(sha1));
		}

		read_end();
	}

	ce = make_cache_entry(0644, sha1, name, 0, 0);
	if (!ce) die("make_cache_entry failed for path '%s'", name);
	add_index_entry(&r->svn_index, ce, ADD_CACHE_OK_TO_ADD);

	strbuf_reset(&buf);
	if (convert_to_git(name, data, sz, &buf, SAFE_CRLF_FALSE)) {
		if (write_sha1_file(buf.buf, buf.len, "blob", sha1)) {
		       	die_errno("write blob");
		}
	}
	ce = make_cache_entry(0644, sha1, name, 0, 0);
	if (!ce) die("make_cache_entry failed for path '%s'", name);
	add_index_entry(&r->git_index, ce, ADD_CACHE_OK_TO_ADD);

	strbuf_release(&buf);
	return;

err:
	die("malformed update");
}

/* returns number of entries removed */
static int remove_index_path(struct index_state* idx, struct strbuf* name) {
	int ret = 0;
	int i = index_name_pos(idx, name->buf, name->len);

	if (i >= 0) {
		/* file */
		cache_tree_invalidate_path(idx->cache_tree, name->buf);
		remove_index_entry_at(idx, i);
		return 1;
	}

	/* we've got to re-lookup the path as a < a.c < a/c */
	strbuf_addch(name, '/');
	i = -index_name_pos(idx, name->buf, name->len) - 1;

	/* directory, index_name_pos returns -first-1
	 * where first is the position the entry would
	 * be added at, and the cache is sorted */
	while (i < idx->cache_nr) {
		struct cache_entry* ce = idx->cache[i];
		if (ce_namelen(ce) < name->len) break;
		if (memcmp(ce->name, name->buf, name->len)) break;

		ce->ce_flags |= CE_REMOVE;
		i++;
		ret++;
	}

	strbuf_setlen(name, name->len - 1);

	if (ret) {
		cache_tree_invalidate_path(idx->cache_tree, name->buf);
		remove_marked_cache_entries(idx);
	}

	return ret;
}

static void read_delete_entry(struct svnref* r, int rev) {
	/* name, [revno], dir-token */
	struct strbuf name = STRBUF_INIT;

	read_name(&name);
	find_svnref_by_path(&name);
	fprintf(stderr, "D %s\n", name.buf);

	remove_index_path(&r->svn_index, &name);
	remove_index_path(&r->git_index, &name);
	if (!name.len) {
		r->delete = 1;
	}
	strbuf_release(&name);
	return;
}


static void read_update(struct svnref* r, int rev) {
	struct strbuf name = STRBUF_INIT;
	struct strbuf srcdata = STRBUF_INIT;
	struct strbuf tgtdata = STRBUF_INIT;
	const char* cmd = NULL;
	void* data = NULL;
	size_t datasz = 0;
	int filedirty = 0;

	read_success(); /* update */
	read_success(); /* report */

	for (;;) {
		/* finish off previous command */
		if (cmd) read_end_response();

		if (read_list()) goto err;

		cmd = read_word();
		if (!cmd || read_list()) goto err;

		if (!strcmp(cmd, "close-edit")) {
			if (name.len) goto err;
			break;

		} else if (!strcmp(cmd, "abort-edit")) {
			die("update aborted");

		} else if (!strcmp(cmd, "open-root")) {
			if (name.len) goto err;

		} else if (!strcmp(cmd, "add-dir")) {
			if (name.len) goto err;
			read_add_dir(r, rev);

		} else if (!strcmp(cmd, "open-file")) {
			if (name.len) goto err;
			read_open_file(r, rev, &name, &data, &datasz);

		} else if (!strcmp(cmd, "add-file")) {
			if (name.len) goto err;
			read_add_file(r, rev, &name, &data, &datasz);

		} else if (!strcmp(cmd, "close-file")) {
			if (!name.len) goto err;

			if (filedirty) {
				read_close_file(r, name.buf, tgtdata.buf, tgtdata.len);
			}

			strbuf_release(&srcdata);
			strbuf_release(&tgtdata);
			strbuf_reset(&name);
			free(data);
			data = NULL;
			datasz = 0;
			filedirty = 0;

		} else if (!strcmp(cmd, "delete-entry")) {
			if (name.len) goto err;
			read_delete_entry(r, rev);

		} else if (!strcmp(cmd, "apply-textdelta")) {
			/* file-token, [base-checksum] */
			int64_t d;
			if (!name.len) goto err;

			filedirty = 1;
			if ((d = deltamore()) > 0) {
				apply_svndiff(&tgtdata, data, datasz, &deltar, &d, &deltaeof);
			}
		}
	}

	read_end_response(); /* end of close-edit */
	read_success();

	free(data);
	strbuf_release(&name);
	strbuf_release(&srcdata);
	strbuf_release(&tgtdata);
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
struct author* defauthor;

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
	int fd = open(git_path("svn-authors"), O_RDONLY);
	if (fd < 0 || fstat(fd, &st)) return;

	p = xmalloc(st.st_size + 1);
	if (xread(fd, p, st.st_size) != st.st_size)
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

static void svn_author_to_git(struct strbuf* author) {
	int i;

	for (i = 0; i < authorn; i++) {
		struct author* a = &authors[i];
		if (!strcasecmp(author->buf, a->user)) {
			strbuf_reset(author);
			strbuf_addf(author, "%s <%s>", a->name, a->mail);
			return;
		}
	}

	die("could not find username '%s' in %s\n"
			"Add a line of the form:\n"
			"%s = Full Name <email@example.com>\n",
			author->buf,
			git_path("svn-authors"),
			author->buf);
}

static struct author* get_object_author(struct object* obj) {
	const char *lb, *le, *mb, *me;
	struct strbuf buf = STRBUF_INIT;
	struct author* ret = NULL;
	char* data = NULL;
	int i;

	if (obj->type == OBJ_COMMIT) {
		struct commit* cmt = (struct commit*) obj;
		parse_commit(cmt);
		lb = strstr(cmt->buffer, "\ncommitter ");
		if (!lb) lb = strstr(cmt->buffer, "\nauthor ");
	} else if (obj->type == OBJ_TAG) {
		enum object_type type;
		unsigned long size;
		data = read_sha1_file(obj->sha1, &type, &size);
		if (!data || type != OBJ_TAG) goto err;
		lb = strstr(data, "\ntagger ");
	} else {
		die("invalid commit object");
	}

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
				git_path("svn-authors"),
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
				git_path("svn-authors"),
				buf.buf);
	}

	strbuf_release(&buf);
	free(data);
	return ret;

err:
	die("can not find author in %s", sha1_to_hex(obj->sha1));
}

static struct commit* latest_svncmt;

static void init_latest(void) {
	unsigned char sha1[20];
	struct commit* cmt;

	latest_svncmt = NULL;
	if (read_ref("refs/svn/latest", sha1) || is_null_sha1(sha1))
		return;

	cmt = lookup_commit(sha1);
	if (!cmt || parse_commit(cmt)) {
		die("invalid latest ref %s", sha1_to_hex(sha1));
	}

	latest_svncmt = cmt;
}

static int latest_rev(void) {
	return latest_svncmt ? parse_svnrev(latest_svncmt) : 0;
}

static int set_latest(struct commit* cmt) {
	struct ref_lock* lk;

	if (cmt == latest_svncmt) {
		return 0;
	}

	lk = lock_ref_sha1("svn/latest", cmt_sha1(latest_svncmt));
	if (!lk || write_ref_sha1(lk, cmt_sha1(cmt), "svn-fetch")) {
		error("failed to update latest ref");
		return 1;
	}

	latest_svncmt = cmt;
	return 0;
}

static int svn_time_to_git(struct strbuf* time) {
	struct tm tm;
	memset(&tm, 0, sizeof(tm));
	if (!strptime(time->buf, "%Y-%m-%dT%H:%M:%S", &tm)) return -1;
	strbuf_reset(time);
	strbuf_addf(time, "%"PRId64, (int64_t) mktime(&tm));
	return 0;
}

static struct commit* create_fetched_commit(struct svnref* r, int rev, const char* author, const char* time, const char* log, int created) {
	static struct strbuf buf = STRBUF_INIT;
	unsigned char sha1[20];

	struct object *gitobj;
	struct commit *gitcmt, *svncmt;
	struct ref_lock* lk = NULL;

	if (!r->git_index.cache_tree)
		r->git_index.cache_tree = cache_tree();
	if (cache_tree_update(r->git_index.cache_tree, r->git_index.cache, r->git_index.cache_nr, 0))
		die("failed to update cache tree");

	if (!r->svn_index.cache_tree)
		r->svn_index.cache_tree = cache_tree();
	if (cache_tree_update(r->svn_index.cache_tree, r->svn_index.cache, r->svn_index.cache_nr, 0))
		die("failed to update cache tree");

	/* Create the commit object.
	 *
	 * SVN can't create tags and branches without a commit,
	 * but git can. In the cases where new refs are just
	 * created without any changes to the tree, we don't add
	 * a commit. This way git commits pushed to svn and
	 * pulled back again look roughly the same.
	 */
	if (r->delete) {
		gitcmt = NULL;

	} else if ((r->istag || created)
		&& r->parent
		&& !hashcmp(r->git_index.cache_tree->sha1, r->parent->tree->object.sha1)
		  ) {
		/* branch/tag has been created/replaced, but the tree hasn't
		 * been changed */
		gitcmt = r->parent;

	} else {
		strbuf_reset(&buf);
		strbuf_addf(&buf, "tree %s\n", sha1_to_hex(r->git_index.cache_tree->sha1));

		if (r->parent) {
			strbuf_addf(&buf, "parent %s\n", cmt_to_hex(r->parent));
		}

		strbuf_addf(&buf, "author %s %s +0000\n", author, time);
		strbuf_addf(&buf, "committer %s %s +0000\n", author, time);

		strbuf_addch(&buf, '\n');
		strbuf_addstr(&buf, log);

		if (write_sha1_file(buf.buf, buf.len, "commit", sha1))
			die("failed to create commit");

		gitcmt = lookup_commit(sha1);
		if (!gitcmt || parse_commit(gitcmt))
			die("failed to parse created commit");
	}

	/* Create the tag object.
	 *
	 * Now we create an annotated tag wrapped around either
	 * the commit the tag was branched from or the wrapper.
	 * Where a tag is later updated, we either recreate this
	 * tag with a new time (no tree change) or create a new
	 * dummy commit whose parent is the old dummy.
	 */
	if (r->delete) {
		gitobj = NULL;

	} else if (r->istag) {
		strbuf_reset(&buf);
		strbuf_addf(&buf, "object %s\n", cmt_to_hex(gitcmt));
		strbuf_addf(&buf, "type commit\n");
		strbuf_addf(&buf, "tag %s\n", r->remote.buf + strlen("refs/tags/"));
		strbuf_addf(&buf, "tagger %s %s +0000\n", author, time);
		strbuf_addch(&buf, '\n');
		strbuf_addstr(&buf, log);

		if (write_sha1_file(buf.buf, buf.len, tag_type, sha1))
			die("failed to create tag");

		gitobj = parse_object(sha1);
		if (!gitobj)
			die("failed to parse created tag");

	} else {
		gitobj = &gitcmt->object;
	}

	/* Create the svn commit */
	strbuf_reset(&buf);
	strbuf_addf(&buf, "tree %s\n", sha1_to_hex(r->svn_index.cache_tree->sha1));

	if (gitcmt || r->svncmt) {
		strbuf_addf(&buf, "parent %s\n", cmt_to_hex(gitcmt ? gitcmt : r->svncmt));
	}

	if (r->svncmt) {
		strbuf_addf(&buf, "parent %s\n", cmt_to_hex(r->svncmt));
	}

	strbuf_addf(&buf, "author %s %s +0000\n", author, time);
	strbuf_addf(&buf, "committer %s %s +0000\n", author, time);
	strbuf_addf(&buf, "revision %d\n", rev);
	strbuf_addf(&buf, "path %s\n", r->svn.buf);
	strbuf_addch(&buf, '\n');

	if (write_sha1_file(buf.buf, buf.len, "commit", sha1))
		die("failed to create svn object");

	svncmt = lookup_commit(sha1);
	if (!svncmt || parse_commit(svncmt))
		die("failed to parse created svn commit");

	/* update the ref */

	fprintf(stderr, "grab ref lock %s %s\n", r->ref.buf, cmt_to_hex(r->svncmt));
	lk = lock_ref_sha1(r->ref.buf + strlen("refs/"), cmt_sha1(r->svncmt));
	if (!lk || write_ref_sha1(lk, cmt_sha1(svncmt), "svn-fetch")) {
		die("failed to update ref %s", r->ref.buf);
	}

	/* update the remote or tag ref */

	if (r->gitobj && !gitobj) {
		if (delete_ref(r->remote.buf, r->gitobj->sha1, 0)) {
			error("failed to delete ref %s", r->remote.buf);
			goto rollback;
		}
	} else if (gitobj) {
		lk = lock_ref_sha1(r->remote.buf + strlen("refs/"),
				r->gitobj ? r->gitobj->sha1 : null_sha1);

		if (!lk || write_ref_sha1(lk, gitobj->sha1, "svn-fetch")) {
			error("failed to update ref %s", r->remote.buf);
			goto rollback;
		}
	}

	r->delete = 0;
	r->gitobj = gitobj;
	r->svncmt = svncmt;
	r->parent = gitcmt;

	fprintf(stderr, "commited %d %s %s\n", rev, r->ref.buf, sha1_to_hex(r->svncmt->object.sha1));
	return svncmt;

rollback:
	if (r->svncmt) {
		lk = lock_ref_sha1(r->ref.buf + strlen("refs/"), cmt_sha1(svncmt));
		if (!lk || write_ref_sha1(lk, cmt_sha1(r->svncmt), "svn-fetch rollback"))
			goto rollback_failed;
	} else if (svncmt) {
		if (delete_ref(r->ref.buf, cmt_sha1(svncmt), 0))
			goto rollback_failed;
	}

	exit(128);

rollback_failed:
	die("failed to rollback %s", r->ref.buf);
}

static void request_commit(struct svnref* r, int rev, struct svnref* copysrc, int copyrev) {
	fprintf(stderr, "request commit %d\n", rev);

	if (!copysrc) {
		copysrc = r;
		copyrev = rev - 1;
	}

	sendf("( switch ( ( %d ) %d:%s true %d:%s/%s ) )\n" /* [rev] target recurse target-url */
		"( set-path ( 0: %d false ) )\n" /* path rev start-empty */
		"( finish-report ( ) )\n"
		"( success ( ) )\n",
			/* switch target rev */
			rev,
			/* switch target */
			(int) copysrc->svn.len,
			copysrc->svn.buf,
			/* switch target-url */
			(int) (strlen(url) + 1 + r->svn.len),
			url,
			r->svn.buf,
			/* set-path rev */
			copyrev
	     );
}

static void request_log(int from, int to) {
	struct strbuf paths = STRBUF_INIT;
	if (!trunk && !branches && !tags) {
		strbuf_addstr(&paths, "0: ");
	}
       	if (trunk) {
		strbuf_addf(&paths, "%d:%s ", (int) strlen(trunk), trunk);
	}
	if (branches) {
		strbuf_addf(&paths, "%d:%s ", (int) strlen(branches), branches);
	}
	if (tags) {
		strbuf_addf(&paths, "%d:%s ", (int) strlen(tags), tags);
	}

	sendf("( log ( ( %s) " /* (path...) */
		"( %d ) ( %d ) " /* start/end revno */
		"true false " /* changed-paths strict-node */
		"0 " /* limit */
		"false " /* include-merged-revisions */
		"revprops ( 10:svn:author 8:svn:date 7:svn:log ) "
		") )\n",
		paths.buf,
		from, /* log start */
		to /* log end */
	     );

	strbuf_release(&paths);
}

struct pending {
	char *buf, *msg, *author, *time;
	struct svnref *ref, *copysrc;
	int rev, copyrev;
};

static int have_next_commit(struct pending* retp) {
	static struct pending *nextv;
	static int nextc, nexta;
	static int64_t rev;

	struct strbuf msg = STRBUF_INIT;
	struct strbuf author = STRBUF_INIT;
	struct strbuf time = STRBUF_INIT;
	struct strbuf name = STRBUF_INIT;
	struct pending* p;

	while (!nextc) {
		int i;

		/* start of log entry */
		if (read_list()) {
			read_done();
			read_success();
			return 0;
		}

		/* start changed path entries */
		if (read_list()) goto err;

		while (!read_list()) {
			const char* s;
			struct svnref *to;

			/* path, A, (copy-path, copy-rev) */
			strbuf_reset(&name);
			read_name(&name);
			to = find_svnref_by_path(&name);
			s = read_word();
			if (!s) goto err;

			for (i = 0; i < nextc; i++) {
				if (nextv[i].ref == to) {
					break;
				}
			}

			if (to && !name.len && (!strcmp(s, "A") || !strcmp(s, "R")) && have_optional()) {
				int copyrev;
				struct svnref* copysrc;

				strbuf_reset(&name);
				copysrc = read_copy_source(&name, &copyrev);
				if (copysrc && name.len) {
					warning("copy from non-root path");
					copysrc = NULL;
				}

				if (i == nextc) {
					ALLOC_GROW(nextv, nextc+1, nexta);
					p = &nextv[nextc++];
				} else {
					p = &nextv[i];
				}

				p->copysrc = copysrc;
				p->copyrev = copyrev;
				p->ref = to;

				read_end();

			} else if (to && i == nextc) {
				ALLOC_GROW(nextv, nextc+1, nexta);
				p = &nextv[nextc++];
				p->copysrc = NULL;
				p->ref = to;
			}

			read_end();
		}

		/* end of changed path entries */
		read_end();

		/* rev number */
		rev = read_number();
		if (rev < 0) goto err;

		/* author */
		if (read_list()) goto err;
		strbuf_reset(&author);
		if (read_strbuf(&author)) goto err;
		svn_author_to_git(&author);
		read_end();

		/* timestamp */
		if (read_list()) goto err;
		strbuf_reset(&time);
		if (read_strbuf(&time)) goto err;
		if (svn_time_to_git(&time)) goto err;
		read_end();

		/* log message */
		strbuf_reset(&msg);
		if (have_optional()) {
			if (read_strbuf(&msg)) goto err;
			strbuf_complete_line(&msg);
			read_end();
		}

		/* end of log entry */
		read_end();
		if (verbose) fputc('\n', stderr);

		/* remove entries where we've already downloaded the
		 * commit */
		for (i = 0; i < nextc;) {
			p = &nextv[i];
			if (p->ref->svncmt && parse_svnrev(p->ref->svncmt) >= rev) {
				memmove(nextv+i, nextv+i+1, nextc - (i+1));
				nextc--;
			} else {
				i++;
				p->buf = xmalloc(msg.len + 1 + author.len + 1 + time.len + 1);

				p->rev = rev;
				p->msg = p->buf;
				p->author = p->msg + msg.len + 1;
				p->time = p->author + author.len + 1;

				memcpy(p->msg, msg.buf, msg.len + 1);
				memcpy(p->author, author.buf, author.len + 1);
				memcpy(p->time, time.buf, time.len + 1);
			}
		}
	}

	*retp = nextv[0];

	memmove(nextv, nextv+1, sizeof(nextv[0])*(nextc-1));
	nextc--;

	strbuf_release(&name);
	strbuf_release(&msg);
	strbuf_release(&author);
	strbuf_release(&time);
	return 1;

err:
	die("malformed log");
}

static char* clean_path(char* p) {
	char* e;
	if (*p == '/') p++;
	e = p + strlen(p);
	if (e > p && e[-1] == '/') e[-1] = '\0';
	return p;
}

static struct author** connection_authors;
static int *svnfdv;
static struct inbuffer *inbufv;

static void setup_globals() {
	int i;

	setenv("TZ", "", 1);

	core_eol = svn_eol;

	if (remotedir) {
		struct strbuf buf = STRBUF_INIT;
		strbuf_addstr(&buf, "refs/remotes/");
		strbuf_addstr(&buf, clean_path((char*) remotedir));
		strbuf_addch(&buf, '/');
		remotedir = strbuf_detach(&buf, NULL);
	} else {
		remotedir = "refs/heads/";
	}

	if (svnfdc < 1) die("invalid number of connections");

	connection_authors = xcalloc(svnfdc+1, sizeof(connection_authors[0]));
	svnfdv = xmalloc((svnfdc+1) * sizeof(svnfdv[0]));
	inbufv = xmalloc((svnfdc+1) * sizeof(inbufv[0]));
	for (i = 0; i <= svnfdc; i++) {
		svnfdv[i] = -1;
		inbufv[i].b = inbufv[i].e = 0;
	}

	parse_authors();

	for (i = 0; svnuser && i < authorn; i++) {
		struct author* a = &authors[i];
		if (!strcasecmp(a->user, svnuser)) {
			defauthor = a;
			if (!a->pass) {
				die("user specified with --user needs a password");
			}
			break;
		}
	}

	if (!defauthor) die("need to specify default user with --user");
	if (!url) die("need to specify a url with --url");

	if (trunk) trunk = clean_path((char*) trunk);
	if (branches) branches = clean_path((char*) branches);
	if (tags) tags = clean_path((char*) tags);

	init_latest();
}

static void close_connection(int cidx) {
	if (svnfdv[cidx] >= 0) {
		close(svnfdv[cidx]);
	}
	svnfdv[cidx] = -1;
	connection_authors[cidx] = NULL;
	inbufv[cidx].b = inbufv[cidx].e = 0;
}

static void change_connection(int cidx, struct author* a) {
	char pathsep;
	char *host, *port, *path;
	struct addrinfo hints, *res, *ai;
	int err;
	int fd;

	svnfd = svnfdv[cidx];
	inbuf = &inbufv[cidx];
	fprintf(stderr, "change_connection %d fd %d oldauth %s newauth %s\n", cidx, svnfd, connection_authors[cidx] ? connection_authors[cidx]->user : NULL, a->user);
	if (svnfd >= 0 && connection_authors[cidx] == a) {
		return;
	}

	close_connection(cidx);

	if (prefixcmp(url, "svn://"))
		die(_("only svn repositories are supported"));

	if (!a->pass)
		die("need a password for user %s", a->user);

	host = (char*) url + strlen("svn://");

	path = strchr(host, '/');
	if (!path) path = host + strlen(host);
	pathsep = *path;
	*path = '\0';

	port = strchr(host, ':');
	if (port) *(port++) = '\0';

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;

	err = getaddrinfo(host, port ? port : "3690", &hints, &res);
	*path = pathsep;
	if (port) port[-1] = ':';

	if (err)
		die_errno("failed to connect to %s", url);

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
		die_errno("failed to connect to %s", url);

	svnfd = svnfdv[cidx] = fd;
	inbuf = &inbufv[cidx];


	/* TODO: client software version and client capabilities */
	sendf("( 2 ( edit-pipeline svndiff1 ) %d:%s )\n( CRAM-MD5 ( ) )\n",
			(int) strlen(url), url);

	/* TODO: we don't care about capabilities/versions right now */
	if (strcmp(read_response(), "success")) die("server error");

	/* minver then maxver */
	if (read_number() > 2 || read_number() < 2)
		die(_("version mismatch"));

	read_end_response();

	/* TODO: read the mech lists et all */
	read_success();

	cram_md5(a->user, a->pass);

	sendf("( reparent ( %d:%s ) )\n", (int) strlen(url), url);

	read_success(); /* auth */
	read_success(); /* repo info */
	read_success(); /* reparent */
	read_success(); /* reparent again */

	connection_authors[cidx] = a;
}

static int run_gc_auto() {
	const char *args[] = {"gc", "--auto", NULL};
	return run_command_v_opt(args, RUN_GIT_CMD);
}

static const char* print_arg(struct strbuf* sb, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	strbuf_reset(sb);
	strbuf_vaddf(sb, fmt, ap);
	return sb->buf;
}

int cmd_svn_fetch(int argc, const char **argv, const char *prefix) {
	int64_t n;
	int from, to, i;
	struct pending *pending;
	struct commit* svncmt = NULL;

	git_config(&config, NULL);

	argc = parse_options(argc, argv, prefix, builtin_svn_fetch_options,
		       	builtin_svn_fetch_usage, 0);

	if (argc)
		usage_msg_opt(_("Too many arguments."),
			builtin_svn_fetch_usage, builtin_svn_fetch_options);

	setup_globals();

	if (getenv("GIT_SVN_FETCH_REPORT_LATEST")) {
		printf("%d\n", latest_rev());
		return 0;
	}

	from = latest_rev() + 1;
	pending = xcalloc(svnfdc, sizeof(pending[0]));

	change_connection(svnfdc, defauthor);
	sendf("( get-latest-rev ( ) )\n");

	read_success(); /* latest rev */
	read_response(); /* latest rev again */
	n = read_number();
	if (n < 0 || n > INT_MAX) die("latest-rev failed");
	read_end_response();

	from = max(from, 1);
	to = min(last_revision, (int) n);

	fprintf(stderr, "rev %d %d\n", from, to);

	/* gc --auto invalidates the object cache. Thus we have to run
	 * it last. For when we want to run the update in multiple
	 * bunches then we spawn off a sub command for all revisions.
	 */
	if (to > from + FETCH_AT_ONCE) {
		struct strbuf revs = STRBUF_INIT;
		struct strbuf conns = STRBUF_INIT;

		while (from <= to) {
			int ret;
			int cmdto = min(from + FETCH_AT_ONCE, to);
			const char* args[] = {
				"svn-fetch",
				"-c", print_arg(&conns, "%d", svnfdc),
				"-r", print_arg(&revs, "%d", cmdto),
				"--user", svnuser,
				verbose ? "-v" : NULL,
				NULL
			};

			ret = run_command_v_opt(args, RUN_GIT_CMD);
			if (ret) return ret;

			from = cmdto;
		}

		return 0;
	}

	if (from > to) {
		return 0;
	}

	change_connection(svnfdc, defauthor);
	request_log(from, to);
	read_success();

	/* start requesting commits until we've filled out pending
	 * commits or run out of commits */
	i = 0;
	while (i < svnfdc) {
		struct pending* p = &pending[i];

		change_connection(svnfdc, defauthor);
		if (!have_next_commit(p)) break;
		change_connection(i, defauthor);
		request_commit(p->ref, p->rev, p->copysrc, p->copyrev);

		i++;
	}

	i = 0;
	for (;;) {
		struct pending* p = &pending[i];

		/* process a commit */
		if (!p->ref) break;

		/* Only update the latest when we've moved onto a new
		 * revision. That way if we fail after the first of two
		 * branch updates in a revision we replay the whole
		 * revision next time. */
		if (svncmt && p->rev > latest_rev()) {
			set_latest(svncmt);
		}

		if (p->copysrc) {
			struct commit* c = find_svncmt(p->copysrc, p->copyrev);
			checkout_svncmt(p->ref, c);
		}

		change_connection(i, defauthor);
		read_update(p->ref, p->rev);
		svncmt = create_fetched_commit(p->ref, p->rev, p->author, p->time, p->msg, p->copysrc != NULL);
		free(p->buf);

		/* then request a new one on that connection */
		change_connection(svnfdc, defauthor);
		if (have_next_commit(p)) {
			change_connection(i, defauthor);
			request_commit(p->ref, p->rev, p->copysrc, p->copyrev);
		} else {
			p->ref = NULL;
		}

		i = (i+1) % svnfdc;
	}

	if (svncmt) {
		set_latest(svncmt);
	}

	return run_gc_auto();
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

/* check that no commits have been inserted on our branch between from
 * (the previous revision at which we saw a change) and to (the revision
 * we just commited) */
static void check_for_svn_commits(struct svnref* r, int from, int to) {
	if (from + 1 <= to) {
		return;
	}

	sendf("( log ( ( %d:%s ) " /* (path...) */
			"( %d ) ( %d ) " /* start/end revno */
			"false false " /* changed-paths strict-node */
			"0 false " /* limit include-merged-revisions */
			"revprops ( ) ) )\n",
		(int) r->svn.len,
		r->svn.buf,
		from + 1,
		to - 1);

	read_success();
	if (!read_list()) {
		die("commits inserted during push");
	}

	read_done();
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
	int off, depth = 0;

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
	struct svnref* r = op->format_callback_data;
	struct cache_entry* ce;
	struct strbuf buf = STRBUF_INIT;
	unsigned char ins[MAX_INS_LEN], *inp = ins;
	unsigned char hdr[5*MAX_VARINT_LEN], *hp = hdr;
	enum object_type type;
	const char* tok;
	void* data;
	size_t sz;
	int dir;

	if (verbose) fprintf(stderr, "change mode %x/%x sha1 %s/%s path %s\n",
			omode, nmode, sha1_to_hex(osha1), sha1_to_hex(nsha1), path);

	/* dont care about changed directories */
	if (!S_ISREG(nmode)) return;

	dir = change_dir(path);

	ce = index_name_exists(&r->svn_index, path, strlen(path), 0);
	if (!ce) {
		/* file exists in git but not in svn */
		return;
	}

	/* TODO make this actually use diffcore */

	data = read_sha1_file(nsha1, &type, &sz);
	if (type != OBJ_BLOB)
		die("unexpected object type for %s", sha1_to_hex(nsha1));

	if (convert_to_working_tree(path, data, sz, &buf)) {
		free(data);
		data = strbuf_detach(&buf, &sz);
	}

	if (write_sha1_file(data, sz, "blob", ce->sha1)) {
		die_errno("blob write");
	}
	r->svn_index.cache_changed = 1;

	inp = encode_instruction(inp, COPY_FROM_NEW, 0, sz);

	hp = encode_varint(hp, 0); /* source off */
	hp = encode_varint(hp, 0); /* source len */
	hp = encode_varint(hp, sz); /* target len */
	hp = encode_varint(hp, inp - ins); /* ins len */
	hp = encode_varint(hp, sz); /* data len */

	tok = ftoken();
	sendf("( open-file ( %d:%s %s %s ( ) ) )\n"
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

	diff_change(op, omode, nmode, osha1, nsha1, path, odsubmodule, ndsubmodule);

	free(data);
}

static void addremove(struct diff_options* op,
		int addrm,
		unsigned mode,
		const unsigned char* sha1,
		const char* path,
		unsigned dsubmodule)
{
	static struct strbuf buf = STRBUF_INIT;
	struct svnref* r = op->format_callback_data;
	int dir;
	size_t plen = strlen(path);

	if (verbose) fprintf(stderr, "addrm %c mode %x sha1 %s path %s\n",
			addrm, mode, sha1_to_hex(sha1), path);

	dir = change_dir(path);

	if (addrm == '-' && S_ISDIR(mode)) {
		strbuf_reset(&buf);
		strbuf_add(&buf, path, plen);
		if (remove_index_path(&r->svn_index, &buf) > 0) {
			sendf("( delete-entry ( %d:%s ( ) %s ) )\n",
				(int) plen, path, dtoken(dir));
		}

	} else if (addrm == '+' && S_ISDIR(mode)) {
		sendf("( add-dir ( %d:%s %s %s ( ) ) )\n",
			(int) plen, path, dtoken(dir), dtoken(dir+1));

		dir_changed(++dir, path);

	} else if (addrm == '-' && S_ISREG(mode)) {
		strbuf_reset(&buf);
		strbuf_add(&buf, path, plen);
		if (remove_index_path(&r->svn_index, &buf) > 0) {
			sendf("( delete-entry ( %d:%s ( ) %s) )\n",
				(int) plen, path, dtoken(dir));
		}

	} else if (addrm == '+' && S_ISREG(mode)) {
		unsigned char ins[MAX_INS_LEN], *inp = ins;
		unsigned char hdr[5*MAX_VARINT_LEN], *hp = hdr;
		struct cache_entry* ce;
		unsigned char nsha1[20];
		struct strbuf buf = STRBUF_INIT;
		enum object_type type;
		const char* tok;
		void* data;
		size_t sz;

		/* files beginning with .git eg .gitempty,
		 * .gitattributes, etc are filtered from svn
		 */
		const char* p = strrchr(path, '/');
		p = p ? p+1 : path;
		if (!prefixcmp(p, ".git")) {
			return;
		}

		hashcpy(nsha1, sha1);
		data = read_sha1_file(nsha1, &type, &sz);
		if (!data || type != OBJ_BLOB)
			die("unexpected object type for %s", sha1_to_hex(sha1));

		if (convert_to_working_tree(path, data, sz, &buf)) {
			free(data);
			data = strbuf_detach(&buf, &sz);

			if (write_sha1_file(data, sz, "blob", nsha1)) {
				die_errno("blob write");
			}
		}

		ce = make_cache_entry(0644, sha1, path, 0, 0);
		add_index_entry(&r->svn_index, ce, ADD_CACHE_OK_TO_ADD);

		inp = encode_instruction(inp, COPY_FROM_NEW, 0, sz);

		hp = encode_varint(hp, 0); /* source off */
		hp = encode_varint(hp, 0); /* source len */
		hp = encode_varint(hp, sz); /* target len */
		hp = encode_varint(hp, inp - ins); /* ins len */
		hp = encode_varint(hp, sz); /* data len */

		/* TODO: use diffcore to find copies */

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

		free(data);
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

static int read_commit_revno(struct strbuf* time) {
	int64_t n;

	read_success();
	read_success();

	/* commit-info */
	if (read_list()) goto err;
	n = read_number();
	if (n < 0 || n > INT_MAX) goto err;
	if (have_optional() && time) {
		read_strbuf(time);
		svn_time_to_git(time);
		read_end();
	}
	read_end();
	if (verbose) fputc('\n', stderr);

	return (int) n;

err:
	die("commit failed");
}

/* returns the rev number */
static int send_commit(struct svnref* r, struct commit* cmt, struct commit* copysrc, const char* log, struct strbuf* time) {
	struct diff_options op;
	int dir;

	fcount = 0;
	curref = r;

	sendf("( commit ( %d:%s ) )\n"
		"( open-root ( ( ) %s ) )\n",
		(int) strlen(log), log,
		dtoken(0));

	read_success();
	read_success();

	dir = change_dir(r->svn.buf);

	/* replace is delete then create in the same revision */

	if ((r->gitobj && !cmt) || (r->gitobj != &r->parent->object)) {
		sendf("( delete-entry ( %d:%s ( ) %s ) )\n",
				(int) r->svn.len,
				r->svn.buf,
				dtoken(dir));
	}

	if (cmt) {
		/* We never have to create the root */
		int create = r->svn.len && (!r->gitobj || r->gitobj != &r->parent->object);

		if (copysrc) {
			struct strbuf path = STRBUF_INIT;
			parse_svnpath(copysrc, &path);

			sendf("( %s ( %d:%s %s %s ( %d:%s %d ) ) )\n",
					create ? "add-dir" : "open-dir",
					(int) r->svn.len,
					r->svn.buf,
					dtoken(dir),
					dtoken(dir+1),
					(int) path.len,
					path.buf,
					parse_svnrev(copysrc));

			strbuf_release(&path);
		} else {
			sendf("( %s ( %d:%s %s %s ( ) ) )\n",
				create ? "add-dir" : "open-dir",
				(int) r->svn.len,
				r->svn.buf,
				dtoken(dir),
				dtoken(dir+1));
		}

		dir_changed(++dir, r->svn.buf);

		diff_setup(&op);
		op.output_format = DIFF_FORMAT_CALLBACK;
		op.change = &change;
		op.add_remove = &addremove;
		op.format_callback = &output;
		op.format_callback_data = r;
		DIFF_OPT_SET(&op, RECURSIVE);
		DIFF_OPT_SET(&op, IGNORE_SUBMODULES);
		DIFF_OPT_SET(&op, TREE_IN_RECURSIVE);

		fprintf(stderr, "diff %s to %s\n", cmt_to_hex(r->parent), cmt_to_hex(cmt));
		if (r->parent) {
			if (diff_tree_sha1(cmt_sha1(r->parent), cmt_sha1(cmt), "", &op))
				die("diff tree failed");
		} else {
			if (diff_root_tree_sha1(cmt_sha1(cmt), "", &op))
				die("diff tree failed");
		}
		diffcore_std(&op);
		diff_flush(&op);
	}

	change_dir("");
	sendf("( close-dir ( %s ) )\n"
		"( close-edit ( ) )\n",
		dtoken(0));

	return read_commit_revno(time);
}

struct push {
	struct push* next;
	struct object* old;
	struct object* new;
	struct svnref* ref;
	struct commit* copysrc;
};

/* returns the rev number, cmt may be NULL if this is a forced ref
 * creation/deletion, in which case tag is used for the author and
 * message or a fallback author/msg is used (for branch creation or
 * branch/tag deletion) */
static int push_commit(struct push* p, struct object* gitobj) {
	static struct strbuf buf = STRBUF_INIT;
	static struct strbuf time = STRBUF_INIT;
	static struct strbuf logbuf = STRBUF_INIT;

	unsigned char sha1[20];
	struct ref_lock* lk;
	const char* log;
	int rev;
	struct svnref *r = p->ref;
	struct author *auth = gitobj ? get_object_author(gitobj) : defauthor;
	struct commit *svncmt, *gitcmt;

	fprintf(stderr, "push_commit %s\n", sha1_to_hex(gitobj ? gitobj->sha1 : null_sha1));

	change_connection(0, auth);

	if (!gitobj) {
		gitcmt = NULL;
		strbuf_reset(&logbuf);
		strbuf_addf(&logbuf, "%s %s",
			r->parent ? "creating" : "removing",
			r->svn.buf);
		log = logbuf.buf;

	} else if (gitobj->type == OBJ_COMMIT) {
		gitcmt = (struct commit*) gitobj;
		find_commit_subject(gitcmt->buffer, &log);

	} else if (gitobj->type == OBJ_TAG && ((struct tag*) gitobj)->tagged->type == OBJ_COMMIT) {
		struct tag *tag = (struct tag*) gitobj;
		gitcmt = (struct commit*) tag->tagged;
		strbuf_reset(&logbuf);
		strbuf_addstr(&logbuf, tag->tag);
		strbuf_setlen(&logbuf, parse_signature(logbuf.buf, logbuf.len));
		log = logbuf.buf;
	} else {
		die("unexpected type");
	}

	strbuf_reset(&time);
	rev = send_commit(r, gitcmt, p->copysrc, log, &time);

	/* If we find any intermediate commits, we die. They
	 * will be picked up the next time the user does a pull.
	 */
	check_for_svn_commits(r, r->svncmt ? parse_svnrev(r->svncmt) : 0, rev);

	if (!r->svn_index.cache_tree)
		r->svn_index.cache_tree = cache_tree();
	if (cache_tree_update(r->svn_index.cache_tree, r->svn_index.cache, r->svn_index.cache_nr, 0))
		die("failed to update cache tree");

	/* create the svn object */

	strbuf_reset(&buf);
	strbuf_addf(&buf, "tree %s\n", sha1_to_hex(r->svn_index.cache_tree->sha1));

	if (gitcmt || r->svncmt) {
		strbuf_addf(&buf, "parent %s\n", cmt_to_hex(gitcmt ? gitcmt : r->svncmt));
	}

	if (r->svncmt) {
		strbuf_addf(&buf, "parent %s\n", cmt_to_hex(r->svncmt));
	}

	strbuf_addf(&buf, "author %s <%s> %s +0000\n", auth->name, auth->mail, time.buf);
	strbuf_addf(&buf, "committer %s <%s> %s +0000\n", auth->name, auth->mail, time.buf);
	strbuf_addf(&buf, "revision %d\n", rev);
	strbuf_addf(&buf, "path %s\n", r->svn.buf);
	strbuf_addch(&buf, '\n');

	if (write_sha1_file(buf.buf, buf.len, "commit", sha1))
		die("failed to create svn commit");

	svncmt = lookup_commit(sha1);
	if (!svncmt || parse_commit(svncmt))
		die("failed to parse created svn commit");

	/* update the ref */

	lk = lock_ref_sha1(r->ref.buf + strlen("refs/"), cmt_sha1(r->svncmt));
	if (!lk || write_ref_sha1(lk, cmt_sha1(svncmt), "svn-push"))
	       	die("failed to grab ref lock for %s", r->ref.buf);

	/* update the remote */

	if (r->istag) {
		/* since tag 'remotes' are stored in refs/tags/, we let
		 * the caller update it */

	} else if (gitcmt) {
		lk = lock_ref_sha1(r->remote.buf + strlen("refs/"), r->gitobj ? r->gitobj->sha1 : null_sha1);
		if (!lk || write_ref_sha1(lk, cmt_sha1(gitcmt), "svn-push"))
			die("failed to update ref %s", r->remote.buf);

	} else if (r->gitobj) {
		if (delete_ref(r->remote.buf, r->gitobj->sha1, 0))
			die("failed to delete ref %s", r->remote.buf);

	}

	r->svncmt = svncmt;
	r->gitobj = gitobj;
	r->parent = gitcmt;
	return rev;
}

/* returns the committed revno */
static int do_push(struct push* p) {
	struct svnref* r = p->ref;
	struct commit* cmt;
	struct rev_info walk;
	int rev = 0;

	if (p->old != r->gitobj) {
		die("non fast-forward for %s", r->ref.buf);
	}

	if (p->new) {
		int have_commits = 0;
		struct object *new = deref_tag(p->new, NULL, 0);

		if (!new) {
			die("invalid tag %s", sha1_to_hex(p->new->sha1));
		}

		init_revisions(&walk, NULL);
		add_pending_object(&walk, new, "to");
		walk.first_parent_only = 1;
		walk.reverse = 1;

		if (p->old) {
			p->old->flags |= UNINTERESTING;
			add_pending_object(&walk, p->old, "from");
		}

		if (p->copysrc) {
			struct object* obj = &svn_commit(p->copysrc)->object;
			obj->flags |= UNINTERESTING;
			add_pending_object(&walk, obj, "from");
			checkout_svncmt(r, p->copysrc);
		}

		if (prepare_revision_walk(&walk))
			die("prepare rev walk failed");

		while ((cmt = get_revision(&walk)) != NULL) {
			/* when updating a tag that has multiple
			 * commits, the first few are wrapped commits
			 * and only the last is a wrapped tag */
			rev = push_commit(p, (&cmt->object == new) ? p->new : &cmt->object);
			p->copysrc = NULL;
			have_commits = 1;
		}

		/* if there were no commits we have to create a fake
		 * commit to create the branch/tag in svn */
		if (!have_commits && !p->old) {
			rev = push_commit(p, p->new);
		}
	} else {
		rev = push_commit(p, NULL);
	}

	return rev;
}

static void new_push(struct push** list, const char* ref, const char* oldref, const char* newref) {
	unsigned char old[20], new[20];
	struct push *p = xcalloc(1, sizeof(*p));

	if (get_sha1(oldref, old))
	       	die("invalid ref %s", oldref);

	if (get_sha1(newref, new))
	       	die("invalid ref %s", newref);

	if (!is_null_sha1(old)) {
		p->old = parse_object(old);
		if (!p->old)
			die("invalid ref %s", oldref);
	}

	if (!is_null_sha1(new)) {
		p->new = parse_object(new);
		if (!p->new)
			die("invalid ref %s", newref);
	}

	p->ref = find_svnref_by_refname(ref);
	p->next = *list;
	*list = p;
}

int cmd_svn_push(int argc, const char **argv, const char *prefix) {
	struct push *updates = NULL, *p;
	char buf[256];
	int latest;

	git_config(&config, NULL);

	argc = parse_options(argc, argv, prefix, builtin_svn_push_options,
			builtin_svn_push_usage, 0);

	setup_globals();

	latest = latest_rev();

	/* get the list of references to push */
	if (pre_receive) {
		if (argc)
			usage_msg_opt( _("Too many arguments."),
				builtin_svn_push_usage, builtin_svn_push_options);


		while (fgets(buf, sizeof(buf), stdin) && strlen(buf) > 82) {
			buf[strlen(buf)-1] = '\0';
			buf[40] = '\0';
			buf[81] = '\0';
			new_push(&updates, &buf[82], &buf[0], &buf[41]);
		}
	} else {
		if (argc != 3)
			usage_msg_opt(argc > 3 ? _("Too many arguments.") : _("Too few arguments"),
				builtin_svn_push_usage, builtin_svn_push_options);

		new_push(&updates, argv[0], argv[1], argv[2]);
	}

	/* modify and delete refs */
	for (p = updates; p != NULL; p = p->next) {
		if (p->old) {
			latest = do_push(p);
		}
	}

	/* add refs - do this last so we can find copy bases in the
	 * modified refs. Note if two added refs have a common commit
	 * branching off of svn then those common commits will be
	 * assigned to whichever ref comes first (i.e. unspecified). */
	for (p = updates; p != NULL; p = p->next) {
		if (!p->old) {
			struct object *obj = deref_tag(p->new, NULL, 0);
			if ((p->new && !obj) || (obj && obj->type != OBJ_COMMIT)) {
				die("invalid tag %s", sha1_to_hex(p->new->sha1));
			}
			if (obj) {
				p->copysrc = find_copy_source((struct commit*) obj, latest);
			}
			latest = do_push(p);
		}

	}

	return 0;
}

int cmd_svn_merge_base(int argc, const char **argv, const char *prefix) {
	struct commit *svncmt, *gitcmt;
	struct strbuf buf = STRBUF_INIT;

	git_config(&config, NULL);

	argc = parse_options(argc, argv, prefix, builtin_svn_merge_base_options,
			builtin_svn_merge_base_usage, 0);
	setup_globals();

	if (argc != 1)
		usage_msg_opt(argc > 1 ? _("Too many arguments.") : _("Too few arguments"),
				builtin_svn_merge_base_usage, builtin_svn_merge_base_options);

	gitcmt = lookup_commit_reference_by_name(argv[0]);
	if (!gitcmt) die("could not find commit for %s", argv[0]);

	svncmt = find_copy_source(gitcmt, latest_rev());
	if (!svncmt) die("could not find merge base for %s", argv[0]);

	parse_svnpath(svncmt, &buf);
	printf("%s@%d %s\n", buf.buf, parse_svnrev(svncmt), sha1_to_hex(svncmt->object.sha1));
	return 0;
}
