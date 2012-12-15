#include "cache.h"

int read_sha1_file_strbuf(struct strbuf *s, const unsigned char *sha1,
		enum object_type *type)
{
	unsigned long size = 0;

	s->buf = read_sha1_file(sha1, type, &size);
	s->alloc = size;
	s->len = size;

	return size;
}

int main(int argc, char **argv)
{
	int success = 1;
	struct strbuf buf = STRBUF_INIT;
	enum object_type type;
	unsigned char sha1[20];

	if (argc < 2)
		return 0;
	if (get_sha1(argv[1], sha1))
		return 0;

	read_sha1_file_strbuf(&buf, sha1, &type);

	printf("Type: %i\n", type);
	printf("Content: '%s'\n", buf.buf);

	return success;
}
