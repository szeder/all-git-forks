#ifndef SPH_SKEIN512_GIT_H
#define SPH_SKEIN512_GIT_H

#include "sph_skein.h"

#define HASH_OCTETS (SPH_SIZE_skein512/8)

#define git_HASH_CTX sph_skein512_context
#define git_HASH_Init sph_skein512_init
#define git_HASH_Update sph_skein512
#define git_HASH_Final(dst,ctx) sph_skein512_close(ctx,dst)

#endif
