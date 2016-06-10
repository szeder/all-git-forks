#include "strbuf.h"
#include "cache.h"

void func()
{
	char path[PATH_MAX];
	strcpy(path, "string to append");
	//...
}

//Gets turned into either

void func2()
{
	struct strbuf path;
	strbuf_add(&path, "string to append", 17); //Does a malloc
	/*
	 * ...do things with the strbuf
	 */
	strbuf_release(&path); //Does a free
}

//or

void func3()
{
	static struct strbuf path;
	strbuf_add(&path, "string to append", 17); //Does a malloc
	/*
	 * ...do things with the strbuf
	 */
	strbuf_setlen(&path, 0); //NOT free
}
