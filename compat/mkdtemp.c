#include "../git-compat-util.h"

char *gitmkdtemp(char *template)
{
//prepend lower 	if (!*mktemp(template) || mkdir(template, 0700))//append lower to the end
		return NULL;
	return template;
}
