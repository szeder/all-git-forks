/*
 * test-tr  A simplified tr(1) implementation for testing purposes
 *
 * It offers a limited set of POSIX tr, in particular: no character
 * class support and no [n*m] operators. Only 8bit. C-escapes
 * supported, and character ranges. Deletion and squeezing should
 * work, but -s does not match the GNU tr from coreutils (which, in
 * turn, does not match POSIX).
 */
#include "cache.h"
#include <poll.h>

static int squeeze, delete;

static unsigned char *unquote(const char *s, unsigned *len)
{
	unsigned char *result = malloc(strlen(s) + 1), *r = result;

	while (*s) {
		switch (*s) {
		case '\\':
			++s;
#define ISOCT(c) (((c) >= '0' && (c) <= '7'))
			if (ISOCT(*s)) {
				unsigned int c;
				char oct[4] = {0,0,0,0};
				oct[0] = *s++;
				c = (oct[0] - '0');
				if (ISOCT(*s)) {
					oct[1] = *s++;
					c = (c << 3) |(oct[1] - '0');
					if (ISOCT(*s)) {
						oct[2] = *s++;
						c = (c << 3) |(oct[2] - '0');
					}
				}
				if (c > 255) {
					fprintf(stderr, "invalid octal character specification: \\%s\n", oct);
					exit(1);
				}
				*r++ = c & 0xff;
			} else {
				switch (*s) {
				case '\0':
					*r++ = '\\';
					break;
				case '\\':
					*r++ = *s++;
					break;
				case 'a':
					*r++ = '\a';
					++s;
					break;
				case 'b':
					*r++ = '\b';
					++s;
					break;
				case 'f':
					*r++ = '\f';
					++s;
					break;
				case 'n':
					*r++ = '\n';
					++s;
					break;
				case 'r':
					*r++ = '\r';
					++s;
					break;
				case 't':
					*r++ = '\t';
					++s;
					break;
				case 'v':
					*r++ = '\v';
					++s;
					break;
				default:
					*r++ = '\\';
					*r++ = *s++;
					break;
				}
			}
			break;
		default:
			*r++ = *s++;
		}
	}

	*len = r - result;
	*r = '\0';
	return result;
}

#define MAX_PATTERN 256
static void put_op(unsigned char *conv, unsigned char ch, unsigned *len)
{
	unsigned i = (*len)++;
	if (*len > MAX_PATTERN) {
		fprintf(stderr, "pattern too long\n");
		exit(1);
	}
	conv[i] = ch;
}

static void parse(const unsigned char *rule, unsigned rule_len,
		  unsigned char *set, unsigned *set_len)
{
	const unsigned char *p = rule;
	while (p < rule + rule_len) {
		if ('-' == *p && p > rule && p[1]) {
			unsigned c;
			if (p[-1] > p[1]) {
				fprintf(stderr, "%c%c%c: range is reversed\n",
					p[-1], *p, p[1]);
				exit(1);
			}
			c = p[-1] + 1u;
			for (; c <= p[1]; ++c)
				put_op(set, c, set_len);
			++p;
			++p;
			continue;
		}
		put_op(set, *p, set_len);
		++p;
	}
}

int main(int argc, char *argv[])
{
	unsigned set1_len = 0, set2_len = 0;
	unsigned char set1[MAX_PATTERN];
	unsigned char set2[MAX_PATTERN];

	ssize_t n;
	unsigned char last = 0, have_last = 0;
	unsigned char buf[BUFSIZ];

	char *rule1 = NULL, *rule2 = NULL;
	unsigned char *urule1, *urule2;
	unsigned urule1_len, urule2_len;
	int opt;

	for (opt = 1; opt < argc; ++opt) {
		if (!strcmp("-s", argv[opt]))
			squeeze = 1;
		else if (!strcmp("-d", argv[opt]))
			delete = 1;
		else if (!rule1) {
			rule1 = argv[opt];
		} else if (!rule2)
			rule2 = argv[opt];
	}
	if (!rule1) {
	    fprintf(stderr, "no source set given\n"
		    "test-tr [-s] [-d] set1 [set2]\n"
		    "\"set\" supports only \\NNN, \\a-\\v and CHAR1-CHAR2 rules\n");
	    exit(1);
	}
	if (delete && rule2) {
		fprintf(stderr, "extra operand %s when deleting\n", rule2);
		exit(1);
	}
	urule1 = unquote(rule1, &urule1_len);
	urule2 = NULL;
	urule2_len = 0;
	if ((!rule2 || !*rule2) && !delete && !squeeze) {
		fprintf(stderr, "set2 must be non-empty\n");
		exit(1);
	}

	parse(urule1, urule1_len, set1, &set1_len);

	if (rule2) {
		unsigned i;
		urule2 = unquote(rule2, &urule2_len);
		parse(urule2, urule2_len, set2, &set2_len);
		i = set2[set2_len - 1];
		while (set2_len < set1_len)
			put_op(set2, i, &set2_len);
	}

	while ((n = read(STDIN_FILENO, buf, sizeof(buf)))) {
		ssize_t wr;
		if (n < 0) {
			int err = errno;
			if (EINTR == err || EAGAIN == err)
				continue;
			fprintf(stderr, "%s: %s\n", argv[0], strerror(err));
			exit(1);
		}
		if (set1_len) {
			unsigned i, o = 0;
			for (i = 0; i < (unsigned)n; ++i) {
				unsigned char *p, ch = buf[i];
				p = memchr(set1, ch, set1_len);
				if (p) {
					if (delete)
						continue;
					if (set2_len)
						ch = set2[p - set1];
				}
				if (!(squeeze && have_last && ch == last))
					buf[o++] = ch;
				have_last = 1;
				last = ch;
			}
			n = o;
		}
		do {
			wr = write(STDOUT_FILENO, buf, n);
			if (wr > 0)
				n -= wr;
			else if (wr == 0)
				exit(0);
			else if (wr < 0) {
				switch (errno)
				{
				case EAGAIN:
					{
						struct pollfd fds;
						fds.fd = STDOUT_FILENO;
						fds.events = POLLOUT;
						poll(&fds, 1, -1);
					}
				case EINTR:
					break;
				default:
					exit(0);
				}
			}
		} while (n);
	}
	return 0;
}
