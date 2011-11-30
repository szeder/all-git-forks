#ifndef SPH_GROESTL512_GIT_H
#define SPH_GROESTL512_GIT_H

#include "sph_groestl.h"

#define HASH_OCTETS (SPH_SIZE_groestl512/8)

#define git_HASH_CTX sph_groestl512_context
#define git_HASH_Init sph_groestl512_init
#define git_HASH_Update sph_groestl512
#define git_HASH_Final(dst,ctx) sph_groestl512_close(ctx,dst)

#endif
