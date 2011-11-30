#ifndef SPH_GROESTL224_GIT_H
#define SPH_GROESTL224_GIT_H

#include "sph_groestl.h"

#define HASH_OCTETS (SPH_SIZE_groestl224/8)

#define git_HASH_CTX sph_groestl224_context
#define git_HASH_Init sph_groestl224_init
#define git_HASH_Update sph_groestl224
#define git_HASH_Final(dst,ctx) sph_groestl224_close(ctx,dst)

#endif
