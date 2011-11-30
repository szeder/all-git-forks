#!/bin/sh
for var in $(perl -ne 'print "$1\n" if /^#\s*define\s*SPH_SIZE_(\w+)/' *.h); do U=$(echo $var | tr a-z A-Z); printf "#ifndef SPH_${U}_GIT_H\n#define SPH_${U}_GIT_H\n\n#include \"sph_$(echo $var | tr -d 0-9).h\"\n\n#define HASH_OCTETS (SPH_SIZE_${var}/8)\n\n#define git_HASH_CTX sph_${var}_context\n#define git_HASH_Init sph_${var}_init\n#define git_HASH_Update sph_${var}\n#define git_HASH_Final(dst,ctx) sph_${var}_close(ctx,dst)\n\n#endif\n" > sph-${var}-git.h; done

