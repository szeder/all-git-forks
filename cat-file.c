#include "cache.h"
#include <string.h>

// Usage: ./cat-file foobar
int main(int argc, char **argv)
{
    // http://nanoappli.com/blog/archives/3539
    // unsigned charというのはデータ型のひとつ。
    // 格納できる値の範囲がcharと異なる。
    // char                  -128 - 127
    // unsigned char          0 - 255
	unsigned char sha1[20];

	char type[20];
	void *buf;
	unsigned long size;
	char template[] = "temp_git_file_XXXXXX";
	int fd;

    //引数が1個の場合のみOK
    //そうでないならエラー扱い
	if (argc != 2) {
		usage("Error:invalid argument"); // exit(1);
    }

    //argv[1]は標準引数１個目を表す。
    //printf("argv[1] = %s\n", argv[1]);
	if (get_sha1_hex(argv[1], sha1)) {
		usage("cat-file: cat-file <sha1>"); // exit(1);
    }

	buf = read_sha1_file(sha1, type, &size);
	if (!buf)
		exit(1);
	fd = mkstemp(template);
	if (fd < 0)
		usage("unable to create tempfile");
	if (write(fd, buf, size) != size)
		strcpy(type, "bad");
	printf("%s: %s\n", template, type);
}
