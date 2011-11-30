#ifndef SPH_SKEIN384_GIT_H
#define SPH_SKEIN384_GIT_H

#include "sph_skein.h"

#define HASH_OCTETS (SPH_SIZE_skein384/8)

#define git_HASH_CTX sph_skein384_context
#define git_HASH_Init sph_skein384_init
#define git_HASH_Update sph_skein384
#define git_HASH_Final(dst,ctx) sph_skein384_close(ctx,dst)

#endif
