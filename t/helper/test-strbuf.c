#include "git-compat-util.h"
#include "strbuf.h"

/*
 * code to test flags usage for strbuf, using
 * strbuf_wrap_preallocated and strbuf_wrap_fixed
 */
int test_not_fixed_and_owned(struct strbuf *sb) 
{
	size_t size, old_alloc;
	char *res;
	char *str_test=malloc(5 * sizeof(char));
	strbuf_grow(sb, 1);
	strcpy(str_test, "test");
	old_alloc = sb->alloc;
	strbuf_grow(sb, 1000);
	if (old_alloc == sb->alloc)
		die("strbuf_grow does not move the buffer memory as expected");

	res = strbuf_detach(sb, &size);
	free(res);
	strcpy(str_test, "test");
	strbuf_attach(sb, (void *)str_test, strlen(str_test), sizeof(str_test));
	res = strbuf_detach(sb, &size);
	free(res);
	strbuf_release(sb);

	if (sb->buf != strbuf_slopbuf)
		die("strbuf_release does not free the buffer");

	return 0;
}

int main (int argc, char *argv[])
{
	size_t size = 1;
	struct strbuf sb;
	char str_test[5] = "test";
	char str_foo[7] = "foo";
	char *temp;

	if (argc != 2)
		usage("test-strbuf mode");
	
	if (!strcmp(argv[1], "grow_release_default")) { 
		/* ~STRBUF_OWNS_MEMORY AND ~STRBUF_FIXED_MEMORY */
		strbuf_init(&sb, 0);
		strbuf_grow(&sb, 0);
		if (sb.buf == strbuf_slopbuf)
			die("strbuf_grow failed to alloc memory");

		strbuf_release(&sb);
		if (sb.buf != strbuf_slopbuf)
			die("strbuf_release does not free the buffer");
	} else if (!strcmp(argv[1], "init_not_fixed")){
		/* STRBUF_OWNS_MEMORY AND ~STRBUF_FIXED_MEMORY with init */
		strbuf_init(&sb, 0);
		return test_not_fixed_and_owned(&sb);
	} else if (!strcmp(argv[1], "preallocated_multiple")) {
		/* STRBUF_OWNS_MEMORY AND ~STRBUF_FIXED_MEMORY prealloc */
		strbuf_wrap_preallocated(&sb, (void *)str_test,
							     strlen(str_test),
								 sizeof(str_test));

		return test_not_fixed_and_owned(&sb);
	} else if (!strcmp(argv[1], "preallocated_NULL")) {
		strbuf_wrap_preallocated(&sb, NULL,
							     0,
								 sizeof(str_test));
	} else if (!strcmp(argv[1], "grow_fixed_basic_failure")) {
		/* ~STRBUF_OWNS_MEMORY AND STRBUF_FIXED_MEMORY expect fail */
		strbuf_wrap_fixed(&sb, (void *)str_foo,
				          strlen(str_foo),
				          sizeof(str_foo));

		strbuf_grow(&sb, 3);
		strbuf_grow(&sb, 1000);
	} else if (!strcmp(argv[1], "grow_fixed_complexe_failure")) {
		/* ~STRBUF_OWNS_MEMORY AND STRBUF_FIXED_MEMORY expect fail */
		strbuf_wrap_fixed(&sb, (void *)str_foo,
						  strlen(str_foo),
						  sizeof(str_foo));
					
		strbuf_grow(&sb, 4);
	} else if (!strcmp(argv[1], "grow_fixed_success")) {
		/* ~STRBUF_OWNS_MEMORY AND STRBUF_FIXED_MEMORY */
		strbuf_wrap_fixed(&sb, (void *)str_foo, strlen(str_foo),
						  sizeof(str_foo));
					
		strbuf_grow(&sb, 3);
	} else if (!strcmp(argv[1], "detach_fixed")) {
		/* ~STRBUF_OWNS_MEMORY AND STRBUF_FIXED_MEMORY */
		strbuf_wrap_fixed(&sb, (void *)str_test, strlen(str_test),
						  sizeof(str_test));
					
		temp = strbuf_detach(&sb, &size);
		if (!strcmp(sb.buf, temp))
			die("strbuf_detach does not reset the buffer");
		free(temp);
	} else if (!strcmp(argv[1], "release_fixed")) {
		strbuf_wrap_fixed(&sb, (void *)str_test, strlen(str_test),
						  sizeof(sb) + 1);

		strbuf_release(&sb);
		if (sb.buf != strbuf_slopbuf)
			die("strbuf_release does not free the buffer");
	} else if (!strcmp(argv[1], "release_free")) {
		strbuf_init(&sb, 0);
		strbuf_grow(&sb, 250);
		strbuf_release(&sb); 
	} else if (!strcmp(argv[1], "grow_overflow")) {
		strbuf_init(&sb, 1000);
		strbuf_grow(&sb, maximum_unsigned_value_of_type((size_t)1));
	}else {
		usage("test-strbuf mode");
	}
	return 0;
}