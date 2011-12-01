#ifndef SHA1_SPH_COMPAT_H
#define SHA1_SPH_COMPAT_H

#define HASH_OCTETS 20

#define git_HASH_CTX	git_SHA_CTX
#define git_HASH_Init	git_SHA1_Init
#define git_HASH_Update	git_SHA1_Update
#define git_HASH_Final	git_SHA1_Final

#endif

