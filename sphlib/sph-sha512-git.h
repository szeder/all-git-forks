#ifndef SPH_SHA512_GIT_H
#define SPH_SHA512_GIT_H

#include "sph_sha.h"

#define HASH_OCTETS (SPH_SIZE_sha512/8)

#define git_HASH_CTX sph_sha512_context
#define git_HASH_Init sph_sha512_init
#define git_HASH_Update sph_sha512
#define git_HASH_Final(dst,ctx) sph_sha512_close(ctx,dst)

#endif
