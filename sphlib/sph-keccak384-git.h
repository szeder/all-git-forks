#ifndef SPH_KECCAK384_GIT_H
#define SPH_KECCAK384_GIT_H

#include "sph_keccak.h"

#define HASH_OCTETS (SPH_SIZE_keccak384/8)

#define git_HASH_CTX sph_keccak384_context
#define git_HASH_Init sph_keccak384_init
#define git_HASH_Update sph_keccak384
#define git_HASH_Final(dst,ctx) sph_keccak384_close(ctx,dst)

#endif
