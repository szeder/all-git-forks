#include "../git-compat-util.h"
#include "win32.h"
#include <conio.h>
#include "../strbuf.h"

#define THREAD_LOCAL _declspec(thread)

#include "mingw.c"
