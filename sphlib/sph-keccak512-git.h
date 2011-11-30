#ifndef SPH_KECCAK512_GIT_H
#define SPH_KECCAK512_GIT_H

#include "sph_keccak.h"

#define HASH_OCTETS (SPH_SIZE_keccak512/8)

#define git_HASH_CTX sph_keccak512_context
#define git_HASH_Init sph_keccak512_init
#define git_HASH_Update sph_keccak512
#define git_HASH_Final(dst,ctx) sph_keccak512_close(ctx,dst)

#endif
