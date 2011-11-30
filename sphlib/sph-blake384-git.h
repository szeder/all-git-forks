#ifndef SPH_BLAKE384_GIT_H
#define SPH_BLAKE384_GIT_H

#include "sph_blake.h"

#define HASH_OCTETS (SPH_SIZE_blake384/8)

#define git_HASH_CTX sph_blake384_context
#define git_HASH_Init sph_blake384_init
#define git_HASH_Update sph_blake384
#define git_HASH_Final(dst,ctx) sph_blake384_close(ctx,dst)

#endif
