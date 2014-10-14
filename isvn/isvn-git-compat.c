#include <sys/types.h>

#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

#include <pthread.h>

#include <svn_client.h>

#include "isvn/isvn-git2.h"
#include "isvn/isvn-internal.h"

static void __attribute__((noreturn))
vdie(bool pr_errno, const char *fmt, va_list ap)
{

	vfprintf(stderr, fmt, ap);

	if (pr_errno)
		fprintf(stderr, ": %s (%d)", strerror(errno), errno);
	fprintf(stderr, "\n\n");
	fprintf(stderr, "(libgit2 last: %d, %s)\n\n",
	    giterr_last() ? giterr_last()->klass : 0,
	    giterr_last() ? giterr_last()->message : "<none>");
	fflush(stderr);

	if (option_debugging)
		abort();
	else
		exit(EX_SOFTWARE);
}

void
die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vdie(false, fmt, ap);
	/* NOT REACHED */
	va_end(ap);
}

void
die_errno(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vdie(true, fmt, ap);
	/* NOT REACHED */
	va_end(ap);
}

struct pool_entry {
	struct hashmap_entry ent;
	size_t len;
	unsigned char data[];
};

static int
pool_entry_cmp(const struct pool_entry *e1, const struct pool_entry *e2, const
    unsigned char *dummy)
{

	return (e1->len != e2->len || memcmp(e1->data, e2->data, e1->len));
}

static struct hashmap intern_map;
static pthread_mutex_t intern_lk = PTHREAD_MUTEX_INITIALIZER;

const void *
memintern(const void *data, size_t len)
{
	struct pool_entry *e, *newent;

	newent = xcalloc(1, sizeof(struct pool_entry) + len + 1);
	hashmap_entry_init(&newent->ent, memhash(data, len));
	newent->len = len;
	memcpy(newent->data, data, len);

	pthread_mutex_lock(&intern_lk);

	/* initialize string pool hashmap */
	if (!intern_map.hm_buckets)
		hashmap_init(&intern_map, (hashmap_cmp_fn) pool_entry_cmp, 0);

	/* lookup interned string in pool */
	e = hashmap_get(&intern_map, newent, NULL);
	if (!e)
		/* not found: add it */
		hashmap_add(&intern_map, newent);
	pthread_mutex_unlock(&intern_lk);

	if (e)
		free(newent);
	else
		e = newent;
	return e->data;
}

/* Maybe not going to defeat determined attackers, but better than nothing.
 * https://emboss.github.io/blog/2012/12/14/breaking-murmur-hash-flooding-dos-reloaded/ */
static uint32_t mmh3_seed;

unsigned
memhash(const void *p, size_t s)
{
	unsigned output[ 16 / sizeof(unsigned) ];
	unsigned merged, i;

	MurmurHash3_128(p, (int)s, mmh3_seed, output);

	/* Condense 128-bit MMH3 output to whatever unsigned is: */
	merged = output[0];
	for (i = 1; i < ARRAY_SIZE(output); i++)
		merged ^= output[i];

	return merged;
}

void
isvn_complete_line(char *buf, size_t sz)
{
	size_t slen;

	slen = strlen(buf);
	if (slen == 0 || slen >= (sz - 1))
		return;

	buf[slen++] = '\n';
	buf[slen] = '\0';
}

/*
 * If buf ends with suffix, return 1 and subtract the length of the suffix
 * from *len. Otherwise, return 0 and leave *len untouched.
 */
int
strip_suffix_mem(const char *buf, size_t *len, const char *suffix)
{
	size_t slen;

	slen = strlen(suffix);
	if (*len < slen)
		return 0;

	if (memcmp(buf + *len - slen, suffix, slen) == 0) {
		*len -= slen;
		return 1;
	}

	return 0;
}

void
md5_fromstr(unsigned char md5[16], const char *str)
{
	git_oid tmpoid = {};
	char zeros[4] = {};
	int rc;

	rc = git_oid_fromstrn(&tmpoid, str, 32);
	if (rc < 0)
		die("%s: invalid hex string: %.*s\n", __func__, 32, str);

	/* INVARIANTS */
	if (memcmp(&tmpoid.id[16], zeros, 4) != 0)
		die("Non-zero remainder?");

	memcpy(md5, tmpoid.id, 16);
}

void
md5_tostr(char str[33], unsigned char md5[16])
{
	static const char hex[] = "0123456789abcdef";

	char *s = str;
	unsigned i;

	for (i = 0; i < 16; i++) {
		*s++ = hex[md5[i] >> 4];
		*s++ = hex[md5[i] & 0xf];
	}
	*s++ = '\0';
}

char *
xstrndup(const char *p, size_t s)
{
	char *r;

	r = xmalloc(s + 1);
	strncpy(r, p, s);
	r[s] = '\0';
	return r;
}

void
isvn_git_compat_init(void)
{
	ssize_t rd;
	int fd;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		die_errno("open(urandom)");

	rd = read(fd, &mmh3_seed, sizeof(mmh3_seed));
	if (rd < 0)
		die_errno("read(urandom)");
	else if (rd != sizeof(mmh3_seed))
		die("read(urandom): short read");

	close(fd);
}
