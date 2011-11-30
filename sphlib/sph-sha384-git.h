#ifndef SPH_SHA384_GIT_H
#define SPH_SHA384_GIT_H

#include "sph_sha.h"

#define HASH_OCTETS (SPH_SIZE_sha384/8)

#define git_HASH_CTX sph_sha384_context
#define git_HASH_Init sph_sha384_init
#define git_HASH_Update sph_sha384
#define git_HASH_Final(dst,ctx) sph_sha384_close(ctx,dst)

#endif
