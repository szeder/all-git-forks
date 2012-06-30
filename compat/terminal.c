#include "git-compat-util.h"
#include "compat/terminal.h"
#include "sigchain.h"
#include "strbuf.h"

#ifdef HAVE_DEV_TTY

static int term_fd = -1;
static struct termios old_term;

static void restore_term(void)
{
	if (term_fd < 0)
		return;

	tcsetattr(term_fd, TCSAFLUSH, &old_term);
	term_fd = -1;
}

static void restore_term_on_signal(int sig)
{
	restore_term();
	sigchain_pop(sig);
	raise(sig);
}

char *git_terminal_prompt(const char *prompt, int echo)
{
	static struct strbuf buf = STRBUF_INIT;
	int r;
	FILE *fh;

	fh = fopen("/dev/tty", "w+");
	if (!fh)
		return NULL;

	if (!echo) {
		struct termios t;

		if (tcgetattr(fileno(fh), &t) < 0) {
			fclose(fh);
			return NULL;
		}

		old_term = t;
		term_fd = fileno(fh);
		sigchain_push_common(restore_term_on_signal);

		t.c_lflag &= ~ECHO;
		if (tcsetattr(fileno(fh), TCSAFLUSH, &t) < 0) {
			term_fd = -1;
			fclose(fh);
			return NULL;
		}
	}

	fputs(prompt, fh);
	fflush(fh);

	r = strbuf_getline(&buf, fh, '\n');
	if (!echo) {
		putc('\n', fh);
		fflush(fh);
	}

	restore_term();
	fclose(fh);

	if (r == EOF)
		return NULL;
	return buf.buf;
}

#elif defined(WIN32)

char *git_terminal_prompt(const char *prompt, int echo)
{
	static struct strbuf buf = STRBUF_INIT;
	int r;
	FILE *input_fh, *output_fh;
	DWORD cmode = 0;
	HANDLE hconin;

	input_fh = fopen("CONIN$", "r");
	if (!input_fh)
		return NULL;

	output_fh = fopen("CONOUT$", "w");
	if (!output_fh) {
		fclose(input_fh);
		return NULL;
	}

	if (!echo) {
		hconin = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE,
		    FILE_SHARE_READ, NULL, OPEN_EXISTING,
		    FILE_ATTRIBUTE_NORMAL, NULL);
		if (hconin == INVALID_HANDLE_VALUE) {
			fclose(input_fh);
			fclose(output_fh);
			return NULL;
		}
		GetConsoleMode(hconin, &cmode);
		if (!SetConsoleMode(hconin, cmode & (~ENABLE_ECHO_INPUT))) {
			fclose(input_fh);
			fclose(output_fh);
			return NULL;
		}
	}

	fputs(prompt, output_fh);
	fflush(output_fh);

	r = strbuf_getline(&buf, input_fh, '\n');
	if (!echo) {
		putc('\n', output_fh);
		fflush(output_fh);

		SetConsoleMode(hconin, cmode);
		CloseHandle(hconin);
	}

	if (buf.buf[buf.len - 1] == '\r')
		strbuf_setlen(&buf, buf.len - 1);

	fclose(input_fh);
	fclose(output_fh);

	if (r == EOF)
		return NULL;
	return buf.buf;
}

#else

char *git_terminal_prompt(const char *prompt, int echo)
{
	return getpass(prompt);
}

#endif
