/*
 * GIT - The information manager from hell
 *
 * Copyright (C) Linus Torvalds, 2005
 */
#include "git-compat-util.h"
#include "cache.h"

typedef void (*emit_fn)(struct strbuf *, void *);

static void v_format(const char *prefix, const char *fmt, va_list params,
		     emit_fn emit, void *cb_data)
{
	struct strbuf buf = STRBUF_INIT;
	struct strbuf line = STRBUF_INIT;
	const char *cp, *np;

	strbuf_vaddf(&buf, fmt, params);
	for (cp = buf.buf; *cp; cp = np) {
		np = strchrnul(cp, '\n');
		/*
		 * TRANSLATORS: the format is designed so that in RTL
		 * languages you could reorder and put the "prefix" at
		 * the end instead of the beginning of a line if you
		 * wanted to.
		 */
		strbuf_addf(&line,
			    _("%s: %.*s\n"),
			    prefix,
			    (int)(np - cp), cp);
		emit(&line, cb_data);
		strbuf_reset(&line);
		if (*np)
			np++;
	}
	strbuf_release(&buf);
	strbuf_release(&line);
}

static void emit_report(struct strbuf *line, void *cb_data)
{
	fprintf(stderr, "%.*s", (int)line->len, line->buf);
}

void vreportf(const char *prefix, const char *err, va_list params)
{
	v_format(prefix, err, params, emit_report, NULL);
}

static void emit_write(struct strbuf *line, void *cb_data)
{
	int *fd = cb_data;
	write_in_full(*fd, line->buf, line->len);
}

void vwritef(int fd, const char *prefix, const char *err, va_list params)
{
	v_format(prefix, err, params, emit_write, &fd);
}

static NORETURN void usage_builtin(const char *err, va_list params)
{
	vreportf("usage", err, params);
	exit(129);
}

static NORETURN void die_builtin(const char *err, va_list params)
{
	vreportf("fatal", err, params);
	exit(128);
}

static void error_builtin(const char *err, va_list params)
{
	vreportf("error", err, params);
}

static void warn_builtin(const char *warn, va_list params)
{
	vreportf("warning", warn, params);
}

/* If we are in a dlopen()ed .so write to a global variable would segfault
 * (ugh), so keep things static. */
static NORETURN_PTR void (*usage_routine)(const char *err, va_list params) = usage_builtin;
static NORETURN_PTR void (*die_routine)(const char *err, va_list params) = die_builtin;
static void (*error_routine)(const char *err, va_list params) = error_builtin;
static void (*warn_routine)(const char *err, va_list params) = warn_builtin;

void set_die_routine(NORETURN_PTR void (*routine)(const char *err, va_list params))
{
	die_routine = routine;
}

void set_error_routine(void (*routine)(const char *err, va_list params))
{
	error_routine = routine;
}

void NORETURN usagef(const char *err, ...)
{
	va_list params;

	va_start(params, err);
	usage_routine(err, params);
	va_end(params);
}

void NORETURN usage(const char *err)
{
	usagef("%s", err);
}

void NORETURN die(const char *err, ...)
{
	va_list params;

	va_start(params, err);
	die_routine(err, params);
	va_end(params);
}

void NORETURN die_errno(const char *fmt, ...)
{
	va_list params;
	char fmt_with_err[1024];
	char str_error[256], *err;
	int i, j;

	err = strerror(errno);
	for (i = j = 0; err[i] && j < sizeof(str_error) - 1; ) {
		if ((str_error[j++] = err[i++]) != '%')
			continue;
		if (j < sizeof(str_error) - 1) {
			str_error[j++] = '%';
		} else {
			/* No room to double the '%', so we overwrite it with
			 * '\0' below */
			j--;
			break;
		}
	}
	str_error[j] = 0;
	snprintf(fmt_with_err, sizeof(fmt_with_err), "%s: %s", fmt, str_error);

	va_start(params, fmt);
	die_routine(fmt_with_err, params);
	va_end(params);
}

int error(const char *err, ...)
{
	va_list params;

	va_start(params, err);
	error_routine(err, params);
	va_end(params);
	return -1;
}

void warning(const char *warn, ...)
{
	va_list params;

	va_start(params, warn);
	warn_routine(warn, params);
	va_end(params);
}
