#include "cache.h"

static const char *usage_msg = "\n"
"  test-date show [time_t]...\n"
"  test-date parse [date]...\n"
"  test-date parse-to-timestamp [date]...\n"
"  test-date approxidate_relative [date]...\n";

static void show_dates(char **argv, struct timeval *now)
{
	struct strbuf buf = STRBUF_INIT;

	for (; *argv; argv++) {
		time_t t = atoi(*argv);
		show_date_relative(t, 0, now, &buf);
		printf("%s -> %s\n", *argv, buf.buf);
	}
	strbuf_release(&buf);
}

static void parse_dates(char **argv, struct timeval *now)
{
	struct strbuf result = STRBUF_INIT;

	for (; *argv; argv++) {
		time_t t;
		int tz;

		strbuf_reset(&result);
		parse_date(*argv, &result);
		if (sscanf(result.buf, "%ld %d", &t, &tz) == 2)
			printf("%s -> %s\n",
			       *argv, show_date(t, tz, DATE_ISO8601));
		else
			printf("%s -> bad\n", *argv);
	}
	strbuf_release(&result);
}

static void parse_to_timestamp(char **argv, struct timeval *now)
{
	struct strbuf result = STRBUF_INIT;

	for (; *argv; argv++) {
		time_t timestamp;
		int offset;

		strbuf_reset(&result);
		if (parse_date_basic(*argv, &timestamp, &offset)) {
			printf("%s -> bad\n", *argv);
		} else {
			printf("%s -> %ld %d\n", *argv, timestamp, offset);
		}
	}
	strbuf_release(&result);
}

static void parse_approxidate_relative(char **argv, struct timeval *now)
{
	for (; *argv; argv++) {
		time_t t;
		t = approxidate_relative(*argv, now);
		printf("%s -> %s\n", *argv, show_date(t, 0, DATE_ISO8601));
	}
}

static void parse_approxidate_careful(char **argv)
{
	for (; *argv; argv++) {
		time_t t;
		t = approxidate_careful(*argv, NULL);
		printf("%s -> %s\n", *argv, show_date(t, -700, DATE_ISO8601));
	}
}

int main(int argc, char **argv)
{
	struct timeval now;
	const char *x;

	x = getenv("TEST_DATE_NOW");
	if (x) {
		now.tv_sec = atoi(x);
		now.tv_usec = 0;
	}
	else
		gettimeofday(&now, NULL);

	argv++;
	if (!*argv)
		usage(usage_msg);
	if (!strcmp(*argv, "show"))
		show_dates(argv+1, &now);
	else if (!strcmp(*argv, "parse"))
		parse_dates(argv+1, &now);
	else if (!strcmp(*argv, "parse-to-timestamp"))
		parse_to_timestamp(argv+1, &now);
	else if (!strcmp(*argv, "approxidate_relative"))
		parse_approxidate_relative(argv+1, &now);
	else if (!strcmp(*argv, "approxidate_careful"))
		parse_approxidate_careful(argv+1);
	else
		usage(usage_msg);
	return 0;
}
