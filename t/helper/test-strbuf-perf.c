/*
 * Benchmark various methods of growing a string. Usage:
 *
 *     ./git-foo <variant> <string> <reps> [<count>]
 *
 * <string> is concatenated into a buffer <reps> times. The whole
 * thing is repeated <count> times.
 *
 * <variant> chooses which variant of the concatenation code to use.
 * See below for details.
 */

#include "git-compat-util.h"
#include "strbuf.h"

int main(int argc, char *argv[])
{
	const char *str;
	size_t len, total_len;
	int reps, count, i;
	char *big_constant_lifetime_buf;
	int variant;

	--argc;
	++argv;

	variant = atoi(*argv++);
	argc--;
	fprintf(stderr, "variant = '%d'\n", variant);

	if (argc) {
		str = *argv++;
		argc--;
		fprintf(stderr, "str = '%s'\n", str);
	} else {
		str = "this is a string that we'll repeatedly insert";
	}
	len = strlen(str);

	if (argc) {
		reps = atoi(*argv++);
		argc--;
	} else {
		reps = 500;
	}
	fprintf(stderr, "reps = '%d'\n", reps);

	total_len = len * reps;

	if (argc) {
		count = atoi(*argv++);
		argc--;
	} else {
		count = 500000000 / reps;
	}
	fprintf(stderr, "count = '%d'\n", count);

	big_constant_lifetime_buf = xmalloc(total_len + 1);
	for (i = 0; i < count; i++) {
		int j;
		if (variant == 0) {
			/* Use buffer allocated a single time */
			char *buf = big_constant_lifetime_buf;

			for (j = 0; j < reps; j++)
				strcpy(buf + j * len, str);
		} else if (variant == 1) {
			/* One correct-sized buffer malloc per iteration */
			char *buf = xmalloc(total_len + 1);

			for (j = 0; j < reps; j++)
				strcpy(buf + j * len, str);

			free(buf);
		} else if (variant == 2) {
			/* Conventional use of strbuf */
			struct strbuf buf = STRBUF_INIT;

			for (j = 0; j < reps; j++)
				strbuf_add(&buf, str, len);

			strbuf_release(&buf);
		} else if (variant == 3) {
			/* strbuf initialized to correct size */
			struct strbuf buf;
			strbuf_init(&buf, total_len);

			for (j = 0; j < reps; j++)
				strbuf_add(&buf, str, len);

			strbuf_release(&buf);
		} else if (variant == 4) {
			/*
			 * Simulated fixed strbuf with correct size.
			 * This code only works because we know how
			 * strbuf works internally, namely that it
			 * will never realloc() or free() the buffer
			 * that we attach to it.
			 */
			struct strbuf buf = STRBUF_INIT;
			strbuf_attach(&buf, big_constant_lifetime_buf, 0, total_len + 1);

			for (j = 0; j < reps; j++)
				strbuf_add(&buf, str, len);

			/* No strbuf_release() here! */
		}
	}
	return 0;
}
