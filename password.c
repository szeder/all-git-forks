#include "cache.h"
#include "git-compat-util.h"
#include "strbuf.h"
#include "run-command.h"

#ifndef WIN32

static void ask_password(struct strbuf *sb, const char *prompt)
{
	struct termios attr;
	tcflag_t c_lflag;

	if (tcgetattr(1, &attr) < 0)
		die("tcgetattr failed!");
	c_lflag = attr.c_lflag;

	attr.c_lflag &= ~(ICANON | ECHO);
	if (tcsetattr(1, 0, &attr) < 0)
		die("tcsetattr failed!");

	fputs(prompt, stderr);

	for (;;) {
		int c = getchar();
		if (c == EOF || c == '\n')
			break;
		strbuf_addch(sb, c);
	}

	fputs("\n", stderr);

	attr.c_lflag = c_lflag;
	if (tcsetattr(1, 0, &attr) < 0)
		die("tcsetattr failed!");
}

#else

static void ask_password(struct strbuf *sb, const char *prompt)
{
	fputs(prompt, stderr);
	for (;;) {
		int c = _getch();
		if (c == '\n' || c == '\r')
			break;
		strbuf_addch(sb, c);
	}
	fputs("\n", stderr);
}

#endif

char *git_getpass(const char *prompt)
{
	const char *askpass;
	struct child_process pass;
	const char *args[3];
	static struct strbuf buffer = STRBUF_INIT;

	strbuf_reset(&buffer);

	askpass = getenv("GIT_ASKPASS");
	if (!askpass)
		askpass = askpass_program;
	if (!askpass)
		askpass = getenv("SSH_ASKPASS");
	if (!askpass || !(*askpass)) {
		ask_password(&buffer, prompt);
		return buffer.buf;
	}

	args[0] = askpass;
	args[1]	= prompt;
	args[2] = NULL;

	memset(&pass, 0, sizeof(pass));
	pass.argv = args;
	pass.out = -1;

	if (start_command(&pass))
		exit(1);

	if (strbuf_read(&buffer, pass.out, 20) < 0)
		die("failed to read password from %s\n", askpass);

	close(pass.out);

	if (finish_command(&pass))
		exit(1);

	strbuf_setlen(&buffer, strcspn(buffer.buf, "\r\n"));

	return buffer.buf;
}
