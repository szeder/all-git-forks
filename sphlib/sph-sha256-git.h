#ifndef SPH_SHA256_GIT_H
#define SPH_SHA256_GIT_H

#include "sph_sha2.h"

#define HASH_OCTETS (SPH_SIZE_sha256/8)

#define git_HASH_CTX sph_sha256_context
#define git_HASH_Init sph_sha256_init
#define git_HASH_Update sph_sha256
#define git_HASH_Final(dst,ctx) sph_sha256_close(ctx,dst)

#endif
