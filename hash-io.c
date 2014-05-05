#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "git-compat-util.h"
#include "hash-io.h"

extern ssize_t write_in_full(int fd, const void *buf, size_t count);

static int write_buf_with_hash(struct hash_context *ctx, int fd)
{
	unsigned int buffered = ctx->write_buffer_len;
	git_SHA1_Update(ctx->sc, ctx->write_buffer, buffered);

	if (write_in_full(fd, ctx->write_buffer, buffered) != buffered)
		return -1;
	ctx->write_buffer_len = 0;
	return 0;
}

int write_with_hash(struct hash_context *ctx, int fd, const void *data, unsigned int len)
{
	while (len) {
		unsigned int buffered = ctx->write_buffer_len;
		unsigned int partial = HASH_IO_WRITE_BUFFER_SIZE - buffered;
		if (partial > len)
			partial = len;
		memcpy(ctx->write_buffer + buffered, data, partial);
		buffered += partial;
		if (buffered == HASH_IO_WRITE_BUFFER_SIZE) {
			ctx->write_buffer_len = buffered;
			if (write_buf_with_hash(ctx, fd))
				return -1;
			buffered = 0;
		}
		ctx->write_buffer_len = buffered;
		len -= partial;
		data = (char *) data + partial;
	}
	return 0;
}

static int write_with_sha1_flush(struct hash_context *ctx, int fd)
{
	unsigned int left = ctx->write_buffer_len;

	if (left) {
		git_SHA1_Update(ctx->sc, ctx->write_buffer, left);
	}

	/* Flush first if not enough space for SHA1 signature */
	if (left + 20 > HASH_IO_WRITE_BUFFER_SIZE) {
		if (write_in_full(fd, ctx->write_buffer, left) != left)
			return -1;
		left = 0;
	}

	/* Append the SHA1 signature at the end */
	git_SHA1_Final(ctx->write_buffer + left, ctx->sc);
	left += 20;
	return (write_in_full(fd, ctx->write_buffer, left) != left) ? -1 : 0;
}


int write_with_hash_flush(struct hash_context *ctx, int fd) {
	return write_with_sha1_flush(ctx, fd);
}

void hash_context_init(struct hash_context *ctx)
{
	ctx->write_buffer_len = 0;
	ctx->sc = xmalloc(sizeof *ctx->sc);
	git_SHA1_Init(ctx->sc);
}

void hash_context_release(struct hash_context *ctx)
{
	free(ctx->sc);
}
