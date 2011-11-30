#ifndef SPH_JH224_GIT_H
#define SPH_JH224_GIT_H

#include "sph_jh.h"

#define HASH_OCTETS (SPH_SIZE_jh224/8)

#define git_HASH_CTX sph_jh224_context
#define git_HASH_Init sph_jh224_init
#define git_HASH_Update sph_jh224
#define git_HASH_Final(dst,ctx) sph_jh224_close(ctx,dst)

#endif
