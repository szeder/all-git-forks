#ifndef SPH_SKEIN256_GIT_H
#define SPH_SKEIN256_GIT_H

#include "sph_skein.h"

#define HASH_OCTETS (SPH_SIZE_skein256/8)

#define git_HASH_CTX sph_skein256_context
#define git_HASH_Init sph_skein256_init
#define git_HASH_Update sph_skein256
#define git_HASH_Final(dst,ctx) sph_skein256_close(ctx,dst)

#endif
