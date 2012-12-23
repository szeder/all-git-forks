#include "builtin.h"

static char *create_temp_file(unsigned char *sha1, int in_tempdir)
{
	static char path[1024];
	static const char template[] = ".merge_file_XXXXXX";
	void *buf;
	enum object_type type;
	unsigned long size;
	int fd;

	buf = read_sha1_file(sha1, &type, &size);
	if (!buf || type != OBJ_BLOB)
		die("unable to read blob object %s", sha1_to_hex(sha1));

	if (in_tempdir) {
		fd = git_mkstemp(path, sizeof(path) - 1, template);
	} else {
		strcpy(path, template);
		fd = xmkstemp(path);
	}
	if (write_in_full(fd, buf, size) != size)
		die_errno("unable to write temp-file");
	close(fd);
	return path;
}

int cmd_unpack_file(int argc, const char **argv, const char *prefix)
{
	unsigned char sha1[20];
	int in_tempdir = 0;

	if (argc < 2 || 3 < argc || !strcmp(argv[1], "-h"))
		usage("git unpack-file [-t] <sha1>");
	if (argc == 3) {
		if (strcmp(argv[1], "-t"))
			usage("git unpack-file [-t] <sha1>");
		in_tempdir = 1;
		argc--;
		argv++;
	}

	if (get_sha1(argv[1], sha1))
		die("Not a valid object name %s", argv[1]);

	git_config(git_default_config, NULL);

	puts(create_temp_file(sha1, in_tempdir));
	return 0;
}
