#ifndef SPH_KECCAK224_GIT_H
#define SPH_KECCAK224_GIT_H

#include "sph_keccak.h"

#define HASH_OCTETS (SPH_SIZE_keccak224/8)

#define git_HASH_CTX sph_keccak224_context
#define git_HASH_Init sph_keccak224_init
#define git_HASH_Update sph_keccak224
#define git_HASH_Final(dst,ctx) sph_keccak224_close(ctx,dst)

#endif
