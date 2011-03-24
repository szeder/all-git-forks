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

void encode_64(struct strbuf *out, const char *data, size_t len)
{
	const char *alpha =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	    "abcdefghijklmnopqrstuvwxyz"
	    "0123456789+/";

	if (!len) {
		strbuf_addch(out, '=');
		return;
	}

	do {
		uint32_t buf = (data[0] << 16) | (data[1] << 8) | data[2];
		char temp[4] = {
			alpha[(buf >> 18) & 63],
			alpha[(buf >> 12) & 63],
			alpha[(buf >>  6) & 63],
			alpha[(buf >>  0) & 63]
		};
		strbuf_add(out, temp, 4);

		data += 3;
		len -= 3;
	} while (len >= 3);

	if (len > 0) {
		int n = 0;
		uint32_t buf  = data[0] << 16;
		if (len > 1) buf |= data[1] << 8;
		if (len > 2) buf |= data[2];

		do {
			strbuf_addch(out, alpha[(buf >> 18) & 63]);
			buf <<= 6;
			n += 6;
		} while (n < len * 8);

		for (; n < 24; n += 6)
			strbuf_addch(out, '=');
	}
}
