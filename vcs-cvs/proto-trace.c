#include "vcs-cvs/proto-trace.h"
#include "git-compat-util.h"
#include "strbuf.h"
#include "cache.h"

static inline const char *strbuf_hex_unprintable(struct strbuf *sb)
{
	static const char hex[] = "0123456789abcdef";
	struct strbuf out = STRBUF_INIT;
	char *c;

	for (c = sb->buf; c < sb->buf + sb->len; c++) {
		if (isprint(*c)) {
			strbuf_addch(&out, *c);
		}
		else {
			strbuf_addch(&out, '\\');
			strbuf_addch(&out, hex[(unsigned char)*c >> 4]);
			strbuf_addch(&out, hex[*c & 0xf]);
		}
	}

	strbuf_swap(sb, &out);
	strbuf_release(&out);
	return sb->buf;
}

static const char *dir_arrow(int direction)
{
	switch (direction) {
	case OUT:
	case OUT_BLOB:
		return "->";
	case IN:
	case IN_BLOB:
		return "<-";
	}
	return NULL;
}

static void proto_trace_blob_kp(const char *trace_key, const char *proto, size_t len, int direction)
{
	struct strbuf out = STRBUF_INIT;

	strbuf_addf(&out, "%s %4zu %s ...BLOB...\n", proto, len, dir_arrow(direction));

	trace_strbuf(trace_key, &out);
	strbuf_release(&out);
}

void proto_trace_kp(const char *trace_key, const char *proto, const char *buf, size_t len, int direction)
{
	int dump_blobs;
	struct strbuf out = STRBUF_INIT;
	struct strbuf **lines, **it;

	if (!trace_want(trace_key))
		return;

	dump_blobs = !!getenv("GIT_TRACE_BLOBS");

	if (!dump_blobs &&
	    (direction == OUT_BLOB ||
	     direction == IN_BLOB)) {
		proto_trace_blob_kp(trace_key, proto, len, direction);
		return;
	}

	lines = strbuf_split_buf(buf, len, '\n', 0);
	for (it = lines; *it; it++) {
		if (it == lines)
			strbuf_addf(&out, "%s %4zu %s %s\n",
				    proto,
				    len,
				    dir_arrow(direction),
				    strbuf_hex_unprintable(*it));
		else
			strbuf_addf(&out, "%s      %s %s\n",
				    proto,
				    dir_arrow(direction),
				    strbuf_hex_unprintable(*it));
	}
	strbuf_list_free(lines);

	trace_strbuf(trace_key, &out);
	strbuf_release(&out);
}

void proto_ztrace_kp(const char *trace_key, const char *proto, size_t len, size_t zlen, int direction)
{
	struct strbuf out = STRBUF_INIT;

	if (!trace_want(trace_key))
		return;

	strbuf_addf(&out, "%s ZLIB %s %zu z%zu\n",
				    proto,
				    dir_arrow(direction),
				    len, zlen);
	trace_strbuf(trace_key, &out);
	strbuf_release(&out);
}

