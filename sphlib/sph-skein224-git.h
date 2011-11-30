#ifndef SPH_SKEIN224_GIT_H
#define SPH_SKEIN224_GIT_H

#include "sph_skein.h"

#define HASH_OCTETS (SPH_SIZE_skein224/8)

#define git_HASH_CTX sph_skein224_context
#define git_HASH_Init sph_skein224_init
#define git_HASH_Update sph_skein224
#define git_HASH_Final(dst,ctx) sph_skein224_close(ctx,dst)

#endif
