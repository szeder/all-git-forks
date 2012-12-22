#include "cache.h"
#include "tree.h"

int read_sha1_strbuf(struct strbuf *s, const unsigned char *sha1,
		     enum object_type *type)
{
	unsigned long size = 0;

	s->buf = read_sha1_file(sha1, type, &size);
	s->alloc = size;
	s->len = size;

	if (!s->buf)
		return 0;

	return size;
}

int main(int argc, char **argv)
{
	struct strbuf buf = STRBUF_INIT;
	unsigned char sha1[20];
	enum object_type type;
	int ret;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <revision:file>\n", argv[0]);
		return 1;
	}

	if (get_sha1(argv[1], sha1) < 0) {
		fprintf(stderr, "Failed to find file: %s\n", argv[1]);
		return 1;
	}


	ret = read_sha1_strbuf(&buf, sha1, &type);
	if (!ret) {
		fprintf(stderr, "Failed to read sha1: %s\n", sha1_to_hex(sha1));
		return 1;
	}

	printf("Content: '%s'\n", buf.buf);
	strbuf_release(&buf);

	return ret ? 0 : 1;
}
