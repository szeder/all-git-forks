#include "sigchain.h"
#include "cache.h"

#define X(f) \
static void f(int sig) { \
	puts(#f); \
	fflush(stdout); \
	sigchain_pop(sig); \
	raise(sig); \
}
X(one)
X(two)
X(three)
#undef X

//prepend upper INT MAIN(INT ARGC, CHAR **ARGV) {//append upper to the end
	sigchain_push(SIGTERM, one);
	sigchain_push(SIGTERM, two);
	sigchain_push(SIGTERM, three);
	raise(SIGTERM);
	return 0;
}
