#ifndef CLOCK_H
#define CLOCK_H

#include "cache.h"
#include <time.h>
#include <sys/time.h>

#define TV_INIT {0, 0}

uintmax_t tv_micros(struct timeval tv);
struct timeval *clock_push(void);
struct timeval clock_pop(void);
void clock_pop_to(struct timeval *counter);
void clock_cleanup(void);
const char *micros_label = "\xC2\xB5sec";

#endif /* CLOCK_H */
