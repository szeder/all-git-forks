#include "git-compat-util.h"
#include "strbuf.h"

/*
 * Check behavior on usual use cases
 */
int test_usual(struct strbuf *sb)
{
	size_t size, old_alloc;
	char *res, *old_buf, *str_test = malloc(5*sizeof(char));
	strbuf_grow(sb, 1);
	strcpy(str_test, "test");
	old_alloc = sb->alloc;
	strbuf_grow(sb, 1000);
	if (old_alloc == sb->alloc)
		die("strbuf_grow does not realloc the buffer as expected");
	old_buf = sb->buf;
	res = strbuf_detach(sb, &size);
	if (res != old_buf)
		die("strbuf_detach does not return the expected buffer");
	free(res);

	strcpy(str_test, "test");
	strbuf_attach(sb, (void *)str_test, strlen(str_test), sizeof(str_test));
	res = strbuf_detach(sb, &size);
	if (res != str_test)
		die("strbuf_detach does not return the expected buffer");
	free(res);
	strbuf_release(sb);

	return 0;
}

int main(int argc, char *argv[])
{
	size_t size = 1;
	struct strbuf sb;
	char str_test[5] = "test";
	char str_foo[7] = "foo";

	if (argc != 2)
		usage("test-strbuf mode");

	if (!strcmp(argv[1], "basic_grow")) {
		/*
		 * Check if strbuf_grow(0) allocate a new NUL-terminated buffer
		 */
		strbuf_init(&sb, 0);
		strbuf_grow(&sb, 0);
		if (sb.buf == strbuf_slopbuf)
			die("strbuf_grow failed to alloc memory");
		strbuf_release(&sb);
		if (sb.buf != strbuf_slopbuf)
			die("strbuf_release does not reinitialize the strbuf");
	} else if (!strcmp(argv[1], "strbuf_check_behavior")) {
		strbuf_init(&sb, 0);
		return test_usual(&sb);
	} else if (!strcmp(argv[1], "grow_overflow")) {
		/*
		 * size_t overflow: should die()
		 */
		strbuf_init(&sb, 1000);
		strbuf_grow(&sb, maximum_unsigned_value_of_type((size_t)1));
	} else {
		usage("test-strbuf mode");
	}

	return 0;
}
