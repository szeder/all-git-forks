#include "tty.h"
#include "stdlib.h"
#include "git-compat-util.h"

/* most GUI terminals set COLUMNS (although some don't export it) */
int term_columns(void)
{
	char *col_string = getenv("COLUMNS");
	int n_cols;

	if (col_string && (n_cols = atoi(col_string)) > 0)
		return n_cols;

#ifdef TIOCGWINSZ
	{
		struct winsize ws;
		// Try to get the number of columns from stdout, but
		// fall back to stdin (e.g. if output is to a pager)
		if (!ioctl(1, TIOCGWINSZ, &ws) || (errno == ENOTTY && !ioctl(0, TIOCGWINSZ, &ws)))
			return ws.ws_col;
	}
#endif

	return 0;
}
