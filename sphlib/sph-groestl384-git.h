#ifndef SPH_GROESTL384_GIT_H
#define SPH_GROESTL384_GIT_H

#include "sph_groestl.h"

#define HASH_OCTETS (SPH_SIZE_groestl384/8)

#define git_HASH_CTX sph_groestl384_context
#define git_HASH_Init sph_groestl384_init
#define git_HASH_Update sph_groestl384
#define git_HASH_Final(dst,ctx) sph_groestl384_close(ctx,dst)

#endif
