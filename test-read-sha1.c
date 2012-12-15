#include "cache.h"
#include "tree.h"

int read_sha1_file_strbuf(struct strbuf *s, const unsigned char *sha1,
		enum object_type *type)
{
	unsigned long size = 0;

	s->buf = read_sha1_file(sha1, type, &size);
	s->alloc = size;
	s->len = size;

	return size;
}

struct read_sha1_file_strbuf_data {
	struct strbuf *s;
	int ret;
};

static int read_file(const unsigned char *sha1, const char *base, int baselen,
		const char *pathname, unsigned mode, int stage, void *context)
{
	struct read_sha1_file_strbuf_data *data = context;
	enum object_type type;

	read_sha1_file_strbuf(data->s, sha1, &type);

	if (type != OBJ_BLOB)
		return READ_TREE_RECURSIVE;

	data->ret = 1;

	return 2;
}

int read_tree_file_strbuf(struct strbuf *s, const unsigned char *sha1,
			  const char *filename)
{
	struct read_sha1_file_strbuf_data data;
	const char *file[] = {NULL, NULL};
	int i;
	struct tree *tree;
	struct pathspec pathspec;

	data.s = s;
	data.ret = 0;

	file[0] = filename;
	init_pathspec(&pathspec, get_pathspec(NULL, file));
	for (i = 0; i < pathspec.nr; i++)
		pathspec.items[i].use_wildcard = 0;

	tree = parse_tree_indirect(sha1);
	if (!tree)
		die("not a tree object");

	read_tree_recursive(tree, "", 0, 0, &pathspec, read_file, &data);
	return data.ret;
}

int main(int argc, char **argv)
{
	struct strbuf buf = STRBUF_INIT;
	unsigned char sha1[20];
	int ret;

	if (argc < 3)
		return 0;

	if (get_sha1(argv[1], sha1))
		return 0;

	ret = read_tree_file_strbuf(&buf, sha1, argv[2]);
	printf("Content: '%s'\n", buf.buf);
	strbuf_release(&buf);

	return ret ? 0 : 1;
}
