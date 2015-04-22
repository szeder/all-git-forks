#include "sha2.h"
#include "sha1.h"
#include "sha2.c"

void blk_SHA1_Init(blk_SHA_CTX *ctx)
{
	SHA384_Init(ctx);
}

void blk_SHA1_Update(blk_SHA_CTX *ctx, const void *dataIn, unsigned long len)
{
	SHA384_Update(ctx, dataIn, len);
}

void blk_SHA1_Final(unsigned char hashout[20], blk_SHA_CTX *ctx)
{
	SHA384_TruncatedFinal(hashout, ctx);
}
