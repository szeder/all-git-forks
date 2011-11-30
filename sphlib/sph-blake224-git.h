#ifndef SPH_BLAKE224_GIT_H
#define SPH_BLAKE224_GIT_H

#include "sph_blake.h"

#define HASH_OCTETS (SPH_SIZE_blake224/8)

#define git_HASH_CTX sph_blake224_context
#define git_HASH_Init sph_blake224_init
#define git_HASH_Update sph_blake224
#define git_HASH_Final(dst,ctx) sph_blake224_close(ctx,dst)

#endif
