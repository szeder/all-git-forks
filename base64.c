#include "cache.h"
#include "strbuf.h"

void decode_64(struct strbuf *out, const char *data, size_t len)
{
	int c, pos = 0, acc = 0;
	do {
		c = *data++;
		if (c == '+')
			c = 62;
		else if (c == '/')
			c = 63;
		else if ('A' <= c && c <= 'Z')
			c -= 'A';
		else if ('a' <= c && c <= 'z')
			c -= 'a' - 26;
		else if ('0' <= c && c <= '9')
			c -= '0' - 52;
		else
			continue; /* garbage */
		switch (pos++) {
		case 0:
			acc = (c << 2);
			break;
		case 1:
			strbuf_addch(out, (acc | (c >> 4)));
			acc = (c & 15) << 4;
			break;
		case 2:
			strbuf_addch(out, (acc | (c >> 2)));
			acc = (c & 3) << 6;
			break;
		case 3:
			strbuf_addch(out, (acc | c));
			acc = pos = 0;
			break;
		}
	} while (--len);
}
