/**
 * Copyright 2013, GitHub, Inc
 * Copyright 2009-2013, Daniel Lemire, Cliff Moon,
 *	David McIntosh, Robert Becho, Google Inc. and Veronika Zenz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "git-compat-util.h"
#include "ewok.h"

int ewah_serialize_native(struct ewah_bitmap *self, int fd)
{
	uint32_t write32;
	size_t to_write = self->buffer_size * 8;

	/* 32 bit -- bit size fr the map */
	write32 = (uint32_t)self->bit_size;
	if (write(fd, &write32, 4) != 4)
		return -1;

	/** 32 bit -- number of compressed 64-bit words */
	write32 = (uint32_t)self->buffer_size;
	if (write(fd, &write32, 4) != 4)
		return -1;

	if (write(fd, self->buffer, to_write) != to_write)
		return -1;

	/** 32 bit -- position for the RLW */
	write32 = self->rlw - self->buffer;
	if (write(fd, &write32, 4) != 4)
		return -1;

	return (3 * 4) + to_write;
}

int ewah_serialize(struct ewah_bitmap *self, int fd)
{
	size_t i;
	eword_t dump[2048];
	const size_t words_per_dump = sizeof(dump) / sizeof(eword_t);

	/* 32 bit -- bit size fr the map */
	uint32_t bitsize =  htonl((uint32_t)self->bit_size);
	if (write(fd, &bitsize, 4) != 4)
		return -1;

	/** 32 bit -- number of compressed 64-bit words */
	uint32_t word_count =  htonl((uint32_t)self->buffer_size);
	if (write(fd, &word_count, 4) != 4)
		return -1;

	/** 64 bit x N -- compressed words */
	const eword_t *buffer = self->buffer;
	size_t words_left = self->buffer_size;

	while (words_left >= words_per_dump) {
		for (i = 0; i < words_per_dump; ++i, ++buffer)
			dump[i] = htonll(*buffer);

		if (write(fd, dump, sizeof(dump)) != sizeof(dump))
			return -1;

		words_left -= words_per_dump;
	}

	if (words_left) {
		for (i = 0; i < words_left; ++i, ++buffer)
			dump[i] = htonll(*buffer);

		if (write(fd, dump, words_left * 8) != words_left * 8)
			return -1;
	}

	/** 32 bit -- position for the RLW */
	uint32_t rlw_pos = (uint8_t*)self->rlw - (uint8_t *)self->buffer;
	rlw_pos = htonl(rlw_pos / sizeof(eword_t));

	if (write(fd, &rlw_pos, 4) != 4)
		return -1;

	return 0;
}

int ewah_read_mmap(struct ewah_bitmap *self, void *map, size_t len)
{
	uint32_t *read32 = map;
	eword_t *read64;
	size_t i;

	self->bit_size = ntohl(*read32++);
	self->buffer_size = self->alloc_size = ntohl(*read32++);
	self->buffer = ewah_realloc(self->buffer, self->alloc_size * sizeof(eword_t));

	if (!self->buffer)
		return -1;

	for (i = 0, read64 = (void *)read32; i < self->buffer_size; ++i) {
		self->buffer[i] = ntohll(*read64++);
	}

	read32 = (void *)read64;
	self->rlw = self->buffer + ntohl(*read32++);

	return (char *)read32 - (char *)map;
}

int ewah_read_mmap_native(struct ewah_bitmap *self, void *map, size_t len)
{
	uint32_t *read32 = map;

	self->bit_size = *read32++;
	self->buffer_size = *read32++;

	if (self->alloc_size)
		free(self->buffer);

	self->alloc_size = 0;
	self->buffer = (eword_t *)read32;

	read32 += self->buffer_size * 2;
	self->rlw = self->buffer + *read32++;

	return (char *)read32 - (char *)map;
}

int ewah_deserialize(struct ewah_bitmap *self, int fd)
{
	size_t i;
	eword_t dump[2048];
	const size_t words_per_dump = sizeof(dump) / sizeof(eword_t);

	ewah_clear(self);

	/* 32 bit -- bit size fr the map */
	uint32_t bitsize;
	if (read(fd, &bitsize, 4) != 4)
		return -1;

	self->bit_size = (size_t)ntohl(bitsize);

	/** 32 bit -- number of compressed 64-bit words */
	uint32_t word_count;
	if (read(fd, &word_count, 4) != 4)
		return -1;

	self->buffer_size = self->alloc_size = (size_t)ntohl(word_count);
	self->buffer = ewah_realloc(self->buffer, self->alloc_size * sizeof(eword_t));

	if (!self->buffer)
		return -1;

	/** 64 bit x N -- compressed words */
	eword_t *buffer = self->buffer;
	size_t words_left = self->buffer_size;

	while (words_left >= words_per_dump) {
		if (read(fd, dump, sizeof(dump)) != sizeof(dump))
			return -1;

		for (i = 0; i < words_per_dump; ++i, ++buffer)
			*buffer = ntohll(dump[i]);

		words_left -= words_per_dump;
	}

	if (words_left) {
		if (read(fd, dump, words_left * 8) != words_left * 8)
			return -1;

		for (i = 0; i < words_left; ++i, ++buffer)
			*buffer = ntohll(dump[i]);
	}

	/** 32 bit -- position for the RLW */
	uint32_t rlw_pos;
	if (read(fd, &rlw_pos, 4) != 4)
		return -1;

	self->rlw = self->buffer + ntohl(rlw_pos);

	return 0;
}
