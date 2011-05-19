#include "../git-compat-util.h"

int tcgetattr(int fd, struct termios *term)
{
	DWORD mode;
	HANDLE fh = (HANDLE)_get_osfhandle(fd);
	if (fh == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}
	if (GetFileType(fh) != FILE_TYPE_CHAR) {
		errno = ENOTTY;
		return -1;
	}

	/* fill in term */
	if (!GetConsoleMode(fh, &mode))
		return error("GetConsoleMode failed!");
	term->c_lflag = (mode & ENABLE_ECHO_INPUT) ? ECHO : 0;

	return 0;
}

int tcsetattr(int fd, int opt_acts, const struct termios *term)
{
	DWORD mode;
	HANDLE fh = (HANDLE)_get_osfhandle(fd);
	if (fh == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}
	if (GetFileType(fh) != FILE_TYPE_CHAR) {
		errno = ENOTTY;
		return -1;
	}
	if (!GetConsoleMode(fh, &mode))
		return error("GetConsoleMode failed!");

	/* change mode */
	mode &= ~ENABLE_ECHO_INPUT;
	mode |= (term->c_lflag & ECHO) ? ENABLE_ECHO_INPUT : 0;
	if (!SetConsoleMode(fh, mode))
		return error("SetConsoleMode failed!");

	if (opt_acts != TCSAFLUSH)
		warning("unsupported action");

	FlushConsoleInputBuffer(fh);

	return 0;
}
