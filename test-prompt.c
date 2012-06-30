#include "git-compat-util.h"
#include "prompt.h"

int main(int argc, char *argv[])
{
	char *user = xstrdup(git_prompt("username: ", PROMPT_ECHO));
	char *pass = xstrdup(git_getpass("password: "));
	printf("username: '%s' (%d)\npassword: '%s' (%d)\n", user,
	    strlen(user), pass, strlen(pass));
	return 0;
}
