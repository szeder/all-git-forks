#include "cache.h"
#include "authors.h"

static const char *usage_msg = "\n"
"  test-authors split [authors_info]\n"
"  test-authors has-multiple-authors [authors]\n";

static void test_split_authors(const char **argv)
{
	struct authors_split split;
	int result;
	struct strbuf splitted = STRBUF_INIT;

	printf("%s -> ",*argv);
	result = split_authors_line(&split, *argv, strlen(*argv));
	if (result)
		printf("error");
	else {
		strbuf_add(&splitted, split.begin, split.end - split.begin);
		printf(splitted.buf);
	}
	printf("\n");
}

static void test_has_multiple_authors(const char **argv)
{
	printf("%s -> %s\n", *argv, has_multiple_authors(*argv) ? "yes" : "no");
}

int cmd_main(int argc, const char **argv)
{
	argv++;
	if (argc != 3)
		usage(usage_msg);
	if (!strcmp(*argv, "split"))
		test_split_authors(argv+1);
	else if (!strcmp(*argv, "has-multiple"))
		test_has_multiple_authors(argv+1);
	else
		usage(usage_msg);
	return 0;
}
