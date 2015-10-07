#ifndef CLOCK_H
#define CLOCK_H

#include "clock.h"
#include "cache.h"

static size_t clock_count = 0;
static size_t clock_alloc = 0;
static struct timeval *clocks;

uintmax_t tv_micros(struct timeval tv)
{
	static const uintmax_t million = 1000000;
	return (tv.tv_sec * (1 * million)) + tv.tv_usec;
}


struct timeval *clock_push(void)
{
	struct timeval *c;
	ALLOC_GROW(clocks, clock_count + 1, clock_alloc);
	c = &clocks[clock_count++];
	gettimeofday(c, NULL);
	return c;
}

struct timeval clock_pop(void)
{
	struct timeval now, difference;
	struct timeval *c;

	if (clock_count == 0) {
		warning("popped clock from empty stack");
		difference.tv_sec = difference.tv_usec = 0;
		return difference;
	}

	gettimeofday(&now, NULL);
	c = &clocks[--clock_count];
	timersub(&now, c, &difference);
	memset(c, 0, sizeof(*c));
	return difference;
}

void clock_pop_to(struct timeval *counter)
{
	struct timeval v1, v2;
	v1 = *counter;
	v2 = clock_pop();
	timeradd(&v1, &v2, counter);
}

void clock_cleanup(void)
{
	if (clock_count > 0)
		warning("bug: %zu clock(s) are on the stack at cleanup",
			clock_count);
}

#endif /* CLOCK_H */
