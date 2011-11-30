#ifndef SPH_JH384_GIT_H
#define SPH_JH384_GIT_H

#include "sph_jh.h"

#define HASH_OCTETS (SPH_SIZE_jh384/8)

#define git_HASH_CTX sph_jh384_context
#define git_HASH_Init sph_jh384_init
#define git_HASH_Update sph_jh384
#define git_HASH_Final(dst,ctx) sph_jh384_close(ctx,dst)

#endif
