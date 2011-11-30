#ifndef SPH_JH256_GIT_H
#define SPH_JH256_GIT_H

#include "sph_jh.h"

#define HASH_OCTETS (SPH_SIZE_jh256/8)

#define git_HASH_CTX sph_jh256_context
#define git_HASH_Init sph_jh256_init
#define git_HASH_Update sph_jh256
#define git_HASH_Final(dst,ctx) sph_jh256_close(ctx,dst)

#endif
