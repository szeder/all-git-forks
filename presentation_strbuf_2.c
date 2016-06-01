#include "strbuf.h"
#include "cache.h"

void func4()
{
	char buf[PATH_MAX];
	struct strbuf path;
	strbuf_wrap_preallocated(&path, buf, "string to append", 17);
	/*
	 * ...do things with the strbuf
	 */
	strbuf_release(&path);
}
