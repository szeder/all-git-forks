#ifndef BUILTIN_GC_H
#define BUILTIN_GC_H

#include "cache.h"


/* Common functions used for gc-like tasks */
const char *lock_repo_for_gc(int force, pid_t* ret_pid);
void remove_pack_on_signal(int signo);

#endif /* BUILTIN_GC_H */
