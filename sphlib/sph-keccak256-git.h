#ifndef SPH_KECCAK256_GIT_H
#define SPH_KECCAK256_GIT_H

#include "sph_keccak.h"

#define HASH_OCTETS (SPH_SIZE_keccak256/8)

#define git_HASH_CTX sph_keccak256_context
#define git_HASH_Init sph_keccak256_init
#define git_HASH_Update sph_keccak256
#define git_HASH_Final(dst,ctx) sph_keccak256_close(ctx,dst)

#endif
