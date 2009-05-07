#ifndef RUN_GIT_H
#define RUN_GIT_H

#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#include "strbuf.h"

int run_git(int argc, const char** argv, struct strbuf* output);

#endif // RUN_GIT_H
