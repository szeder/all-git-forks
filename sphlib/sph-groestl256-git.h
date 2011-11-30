#ifndef SPH_GROESTL256_GIT_H
#define SPH_GROESTL256_GIT_H

#include "sph_groestl.h"

#define HASH_OCTETS (SPH_SIZE_groestl256/8)

#define git_HASH_CTX sph_groestl256_context
#define git_HASH_Init sph_groestl256_init
#define git_HASH_Update sph_groestl256
#define git_HASH_Final(dst,ctx) sph_groestl256_close(ctx,dst)

#endif
