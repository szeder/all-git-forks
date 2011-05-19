#include "git-compat-util.h"
int main(int argc, char *argv[])
{
	_setmode(0, _O_TEXT);
	_setmode(1, _O_TEXT);
	_setmode(2, _O_TEXT);
	printf("'%s'\n", getpass("prompt:"));
	return 0;
}
