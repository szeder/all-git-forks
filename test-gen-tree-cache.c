#include "cache.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <zlib.h>
#include <unistd.h>
#include <stdint.h>

int encode_varint(unsigned int value, char *buf)
{
	unsigned char varint[16];
	unsigned pos = sizeof(varint) - 1;
	varint[pos] = value & 127;
	while (value >>= 7)
		varint[--pos] = 128 | (--value & 127);
	memcpy(buf, varint + pos, sizeof(varint) - pos);
	return sizeof(varint) - pos;
}

static uint32_t output_len = 0;

static void output(void *buf, unsigned long size)
{
	assert(write(1, buf, size) == size);
	output_len += size;
}

void finish(char *zin, int nr_zin)
{
	z_stream z;
	char buf[4096];
	nr_zin += encode_varint(0, zin + nr_zin);
	output(zin, nr_zin);
	return;
	memset(&z, 0, sizeof(z));
	assert(deflateInit(&z, Z_BEST_COMPRESSION) == Z_OK);
	z.avail_in = nr_zin;
	z.next_in = zin;
	z.avail_out = sizeof(buf);
	z.next_out = buf;
	assert(deflate(&z, Z_FINISH) == Z_STREAM_END);
	assert(deflateEnd(&z) == Z_OK);
	output(buf, sizeof(buf) - z.avail_out);
	fprintf(stderr, "%d -> %d\n", nr_zin, (char*)z.next_out - buf);
}

struct idx_entry {
	unsigned char sha1[20];
	uint32_t ptr;
};

int idx_entry_cmp(const void *_a, const void *_b)
{
	struct idx_entry *a = (struct idx_entry *)_a;
	struct idx_entry *b = (struct idx_entry *)_b;
	return memcmp(a->sha1, b->sha1, 20);
}

int main(int ac, char **av)
{
#define HASH_NR (64 * 1024 * 1024) //16769023
	void **hash;
	int nr_hash = 0;

#define PATHBUF_S (1024 * 1024)
	char *pathbuf;
	int pathbuf_len;

#if 0
#define ENTRYBUF_S (64 * 1024 * 1024)
	char *entrybuf;
#endif
	char **entries;
	int nr_entries, entrybuf_len;

	struct idx_entry *idx_entries, *e;
	unsigned long nr_idx = 0;
	uint32_t array[256];

	char buf[1024];
	char zin[4096];
	int i, zin_len, started, cnt = 0;

	unsigned long entry_start, path_start;

	pathbuf = malloc(PATHBUF_S);
	pathbuf_len = 0;

#if 0
	entrybuf = malloc(ENTRYBUF_S);
	entrybuf_len = 0;
#endif

	entries = malloc(sizeof(*entries) * HASH_NR);
	idx_entries = malloc(sizeof(*idx_entries) * HASH_NR);
	nr_entries = 0;

	hash = malloc(sizeof(*hash) * HASH_NR);
	memset(hash, 0, sizeof(*hash) * HASH_NR);
	nr_hash = 0;

	/*
	 * collect unique tree entry lines, index them and save trees
	 * as a sequence of indices instead of real tree entries
	 */
	for (started = 0; fgets(buf, sizeof(buf), stdin);) {
		unsigned int key, idx;
		int len;

		if (!strncmp(buf, "tree ", 5)) {
			if (started)
				finish(zin, zin_len);
			else
				started = 1;
			zin_len = 0;

			assert(!get_sha1_hex(buf + 5, idx_entries[nr_idx].sha1));
			idx_entries[nr_idx].ptr = output_len;
			nr_idx++;
			assert(nr_idx < HASH_NR);
			continue;
		}

		len = strlen(buf);
		if (buf[len-1] == '\n')
			buf[len--] = '\0';
		key = crc32(0, buf, len) % HASH_NR;
		if (hash[key] && !strcmp(*(char**)hash[key], buf))
			idx = (char**)hash[key] - entries;
		else {
			while (hash[key])
				key = (key + 1) % HASH_NR;
			idx = nr_entries++;
			assert(nr_entries < HASH_NR);
			entries[idx] = strdup(buf);
			hash[key] = entries + idx;
			assert((nr_hash++) * 2 < HASH_NR);
		}
		/* reserve 0 for tree terminator */
		zin_len += encode_varint(idx + 1, zin + zin_len);
		assert(zin_len < sizeof(zin));

	}
	if (started)
		finish(zin, zin_len);

	entry_start = output_len;

	/*
	 * convert tree entry members except pathname into binary
	 */

	output(&i, 4);	/* 0th entry will never be used */
	for (i = 0; i < nr_entries; i++) {
		char *s = entries[i];
		long int mode = strtol(s, NULL, 8);
		output(&mode, 4);
	}
	output(entries, 20);	/* 0th entry will never be used */
	for (i = 0; i < nr_entries; i++) {
		char *s = entries[i];
		unsigned char sha1[20];
		get_sha1_hex(s + 12, sha1);
		output(sha1, 20);
	}

	memset(hash, 0, sizeof(*hash) * HASH_NR);
	nr_hash = 0;

	/*
	 * convert tree entries into binary, cut pathname out, to be
	 * stored separately.
	 */

	output(&i, 4);	/* 0th entry will never be used */
	for (i = 0; i < nr_entries; i++) {
		char *path = entries[i] + 53;
		unsigned int key, path_offset;
		int len = strlen(path);
		key = crc32(0, path, len) % HASH_NR;
		if (hash[key] && !strcmp(hash[key], path))
			path_offset = (char*)hash[key] - pathbuf;
		else {
			path_offset = pathbuf_len;
			while (hash[key])
				key = (key + 1) % HASH_NR;
			memcpy(pathbuf + pathbuf_len, path, len + 1);
			pathbuf_len += len + 1;
			assert(pathbuf_len < PATHBUF_S);
			hash[key] = pathbuf + path_offset;
			assert((nr_hash++) * 2 < HASH_NR);
		}
		output(&path_offset, 4);
		free(entries[i]);
	}
	free(entries);

	path_start = output_len;

	output(pathbuf, pathbuf_len);
	free(pathbuf);

	/* Write out the index */
	assert(write(2, &entry_start, 4) == 4);
	nr_entries++;		/* account for 0-th entry too */
	assert(write(2, &nr_entries, 4) == 4);
	assert(write(2, &path_start, 4) == 4);

	qsort(idx_entries, nr_idx, sizeof(*idx_entries), idx_entry_cmp);
	for (i = 0, e = idx_entries; i < 256; i++) {
		while (e < idx_entries + nr_idx) {
			if (e->sha1[0] != i)
				break;
			e++;
		}
		array[i] = e - idx_entries;
	}
	assert(write(2, array, sizeof(array)) == sizeof(array));
	assert(write(2, idx_entries, sizeof(*idx_entries) * nr_idx) == sizeof(*idx_entries) * nr_idx);
	free(idx_entries);

	return 0;
}
