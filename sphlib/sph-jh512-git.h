#ifndef SPH_JH512_GIT_H
#define SPH_JH512_GIT_H

#include "sph_jh.h"

#define HASH_OCTETS (SPH_SIZE_jh512/8)

#define git_HASH_CTX sph_jh512_context
#define git_HASH_Init sph_jh512_init
#define git_HASH_Update sph_jh512
#define git_HASH_Final(dst,ctx) sph_jh512_close(ctx,dst)

#endif
