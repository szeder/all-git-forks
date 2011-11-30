#ifndef SPH_BLAKE256_GIT_H
#define SPH_BLAKE256_GIT_H

#include "sph_blake.h"

#define HASH_OCTETS (SPH_SIZE_blake256/8)

#define git_HASH_CTX sph_blake256_context
#define git_HASH_Init sph_blake256_init
#define git_HASH_Update sph_blake256
#define git_HASH_Final(dst,ctx) sph_blake256_close(ctx,dst)

#endif
