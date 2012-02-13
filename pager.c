#include "cache.h"
#include "run-command.h"
#include "sigchain.h"
#include "column.h"

#ifndef DEFAULT_PAGER
#define DEFAULT_PAGER "less"
#endif

static int spawned_pager;
static int max_columns;

static int retrieve_terminal_width(void)
{
#ifdef TIOCGWINSZ
	struct winsize ws;
	if (ioctl(1, TIOCGWINSZ, &ws))  /* e.g., ENOSYS */
		return 0;
	return ws.ws_col;
#else
	return 0;
#endif
}

/*
 * This is split up from the rest of git so that we can do
 * something different on Windows.
 */

#ifndef WIN32
static void pager_preexec(void)
{
	/*
	 * Work around bug in "less" by not starting it until we
	 * have real input
	 */
	fd_set in;

	FD_ZERO(&in);
	FD_SET(0, &in);
	select(1, &in, NULL, &in, NULL);
}
#endif

static const char *pager_argv[] = { NULL, NULL };
static struct child_process pager_process;

static void wait_for_pager(void)
{
	fflush(stdout);
	fflush(stderr);
	/* signal EOF to pager */
	close(1);
	close(2);
	finish_command(&pager_process);
}

static void wait_for_pager_signal(int signo)
{
	wait_for_pager();
	sigchain_pop(signo);
	raise(signo);
}

const char *git_pager(int stdout_is_tty)
{
	const char *pager;

	if (!stdout_is_tty)
		return NULL;

	pager = getenv("GIT_PAGER");
	if (!pager) {
		if (!pager_program)
			git_config(git_default_config, NULL);
		pager = pager_program;
	}
	if (!pager)
		pager = getenv("PAGER");
	if (!pager)
		pager = DEFAULT_PAGER;
	else if (!*pager || !strcmp(pager, "cat"))
		pager = NULL;

	return pager;
}

void setup_pager(void)
{
	const char *pager = git_pager(isatty(1));
	int width;

	if (!pager || pager_in_use())
		return;

	setenv("GIT_PAGER_IN_USE", "true", 1);

	width = retrieve_terminal_width();
	if (width)
		max_columns = width;

	/* spawn the pager */
	pager_argv[0] = pager;
	pager_process.use_shell = 1;
	pager_process.argv = pager_argv;
	pager_process.in = -1;
	if (!getenv("LESS")) {
		static const char *env[] = { "LESS=FRSX", NULL };
		pager_process.env = env;
	}
#ifndef WIN32
	pager_process.preexec_cb = pager_preexec;
#endif
	if (start_command(&pager_process))
		return;

	/* original process continues, but writes to the pipe */
	dup2(pager_process.in, 1);
	if (isatty(2))
		dup2(pager_process.in, 2);
	close(pager_process.in);

	/* this makes sure that the parent terminates after the pager */
	sigchain_push_common(wait_for_pager_signal);
	atexit(wait_for_pager);
}

int pager_in_use(void)
{
	const char *env;
	env = getenv("GIT_PAGER_IN_USE");
	return env ? git_config_bool("GIT_PAGER_IN_USE", env) : 0;
}

int term_columns(void)
{
	char *col_string = getenv("COLUMNS");
	int n_cols;

	if (col_string && (n_cols = atoi(col_string)) > 0)
		return n_cols;

	if (spawned_pager && max_columns)
		return max_columns;

	n_cols = retrieve_terminal_width();
	return n_cols ? n_cols : 80;
}
