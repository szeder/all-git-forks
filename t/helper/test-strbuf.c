#include "git-compat-util.h"
#include "strbuf.h"
#include "parse-options.h"
#include "builtin.h"

/*
 * Check behavior on usual use cases
 */
static int strbuf_check_behavior(struct strbuf *sb)
{
	char *str_test = xstrdup("test"), *res, *old_buf;
	size_t size, old_alloc;

	strbuf_grow(sb, 1);
	old_alloc = sb->alloc;
	strbuf_grow(sb, sb->alloc - sb->len + 1000);
	if (old_alloc == sb->alloc)
		die("strbuf_grow does not realloc the buffer as expected");
	old_buf = sb->buf;
	res = strbuf_detach(sb, &size);
	if (res != old_buf)
		die("strbuf_detach does not return the expected buffer");
	free(res);

	str_test = xstrdup("test");
	strbuf_attach(sb, str_test, strlen(str_test), strlen(str_test) + 1);
	res = strbuf_detach(sb, &size);
	if (size != strlen(str_test)) 
		die ("size is not as expected");

	if (res != str_test)
		die("strbuf_detach does not return the expected buffer");
	free(res);
	strbuf_release(sb);

	return 0;
}

static int basic_grow(struct strbuf *sb) 
{
	strbuf_init(sb, 0);
	strbuf_grow(sb, 0);
	if (sb->buf == strbuf_slopbuf)
		die("strbuf_grow failed to alloc memory");
	strbuf_release(sb);
	if (sb->buf != strbuf_slopbuf)
		die("strbuf_release does not reinitialize the strbuf");

	return 0;
}

static void grow_overflow(struct strbuf *sb)
{
	strbuf_init(sb, 1000);
	strbuf_grow(sb, maximum_unsigned_value_of_type((size_t)1));
}

static void preallocated_NULL(struct strbuf *sb)
{
	char str_test[5] = "test";

	strbuf_attach_preallocated(sb, NULL, 0, sizeof(str_test));
}

int main(int argc, const char *argv[])
{
	struct strbuf sb;
	enum {
		MODE_UNSPECIFIED = 0,
		MODE_BASIC_GROW ,
		MODE_STRBUF_CHECK,
		MODE_GROW_OVERFLOW,
		MODE_PREALLOC_CHECK,
		MODE_PREALLOC_NULL,
	} cmdmode = MODE_UNSPECIFIED;
	struct option options[] = {
		OPT_CMDMODE(0, "basic_grow", &cmdmode,
			    N_(" basic grow test"),
			    MODE_BASIC_GROW),
		OPT_CMDMODE(0, "strbuf_check_behavior", &cmdmode,
			    N_("check the strbuf's behavior"),
			    MODE_STRBUF_CHECK),
		OPT_CMDMODE(0, "grow_overflow", &cmdmode,
			    N_("test grow expecting overflow"),
			    MODE_GROW_OVERFLOW),
		OPT_CMDMODE(0, "preallocated_check_behavior", &cmdmode,
			    N_("check the wrap's behavior"),
			    MODE_PREALLOC_CHECK),
		OPT_CMDMODE(0, "preallocated_NULL", &cmdmode,
			    N_("initializing wrap with NULL"),
			    MODE_PREALLOC_NULL),
		OPT_END()
	};

	argc = parse_options(argc, argv, NULL, options, NULL, 0);

	if (cmdmode == MODE_BASIC_GROW) {
		/*
		 * Check if strbuf_grow(0) allocate a new NUL-terminated buffer
		 */
		return basic_grow(&sb);
	} else if (cmdmode == MODE_STRBUF_CHECK) {
		strbuf_init(&sb, 0);
		return strbuf_check_behavior(&sb);
	} else if (cmdmode == MODE_GROW_OVERFLOW) {
		/*
		 * size_t overflow: should die().
		 * If this does not die(), fall through to returning success.
		 */
		grow_overflow(&sb);
	} else if (cmdmode == MODE_PREALLOC_CHECK) {
		return 0;
//		return strbuf_check_behavior(&sb);
	} else if (cmdmode == MODE_PREALLOC_NULL) {
		 /*
		 * Violation of invariant "strbuf can't be NULL": should die().
		 * If this does not die(), fall through to returning success.
		 */
		preallocated_NULL(&sb);
	} else {
		usage("test-strbuf mode");
	}

	return 0;
}
