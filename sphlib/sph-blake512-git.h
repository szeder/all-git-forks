#ifndef SPH_BLAKE512_GIT_H
#define SPH_BLAKE512_GIT_H

#include "sph_blake.h"

#define HASH_OCTETS (SPH_SIZE_blake512/8)

#define git_HASH_CTX sph_blake512_context
#define git_HASH_Init sph_blake512_init
#define git_HASH_Update sph_blake512
#define git_HASH_Final(dst,ctx) sph_blake512_close(ctx,dst)

#endif
