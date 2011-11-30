#ifndef SPH_SHA224_GIT_H
#define SPH_SHA224_GIT_H

#include "sph_sha.h"

#define HASH_OCTETS (SPH_SIZE_sha224/8)

#define git_HASH_CTX sph_sha224_context
#define git_HASH_Init sph_sha224_init
#define git_HASH_Update sph_sha224
#define git_HASH_Final(dst,ctx) sph_sha224_close(ctx,dst)

#endif
