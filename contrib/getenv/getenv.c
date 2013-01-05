#include <gnu/lib-names.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Global symbols for easy access from gdb */
static char *getenv_current;
static char *getenv_prev;

/*
 * Intercept standard getenv() via LD_PRELOAD. The return value is
 * made inaccessible by the next getenv() call. This helps catch
 * places that ignore the statement "The string pointed to may be
 * overwritten by a subsequent call to getenv()" [1].
 *
 * The backtrace is appended after the env string, which may be
 * helpful to identify where this getenv() is called in a core dump.
 *
 * [1] http://pubs.opengroup.org/onlinepubs/9699919799/functions/getenv.html
 */
char *getenv(const char *name)
{
	static char *(*libc_getenv)(const char*);
	char *value;

	if (!libc_getenv) {
		void *libc = dlopen(LIBC_SO, RTLD_LAZY);
		libc_getenv = dlsym(libc, "getenv");
	}
	if (getenv_current) {
		mprotect(getenv_current, strlen(getenv_current) + 1, PROT_NONE);
		getenv_prev = getenv_current;
		getenv_current = NULL;
	}

	value = libc_getenv(name);
	if (value) {
		int len = strlen(value) + 1;
		int backtrace_len = 0;
		void *buffer[100];
		char **symbols;
		int i, n;

		n = backtrace(buffer, 100);
		symbols = backtrace_symbols(buffer, n);
		if (symbols) {
			for (i = 0; i < n; i++)
				backtrace_len += strlen(symbols[i]) + 1; /* \n */
			backtrace_len++; /* NULL */
		}

		getenv_current = mmap(NULL, len + backtrace_len,
				      PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
		memcpy(getenv_current, value, len);
		value = getenv_current;

		if (symbols) {
			char *p = getenv_current + len;
			for (i = 0; i < n; i++)
				p += sprintf(p, "%s\n", symbols[i]);
		}
	}
	return value;
}
