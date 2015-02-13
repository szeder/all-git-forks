/*
 * Code to parse pack v4 object encoding
 *
 * (C) Nicolas Pitre <nico@fluxnic.net>
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "cache.h"
#include "tree-walk.h"
#include "packv4-parse.h"
#include "varint.h"

const unsigned char *get_sha1ref(struct packed_git *p,
				 const unsigned char **bufp)
{
	const unsigned char *sha1;

	if (!p->sha1_table)
		return NULL;

	if (!**bufp) {
		sha1 = *bufp + 1;
		*bufp += 21;
	} else {
		unsigned int index = decode_varint(bufp);
		if (index < 1 || index - 1 > p->num_objects) {
			error("bad index in get_sha1ref");
			return NULL;
		}
		sha1 = p->sha1_table + (index - 1) * 20;
	}

	return sha1;
}

struct packv4_dict *pv4_create_dict(const unsigned char *data, int dict_size)
{
	struct packv4_dict *dict;
	int i;

	/* count number of entries */
	int nb_entries = 0;
	const unsigned char *cp = data;
	while (cp < data + dict_size - 3) {
		cp += 2;  /* prefix bytes */
		cp += strlen((const char *)cp);  /* entry string */
		cp += 1;  /* terminating NUL */
		nb_entries++;
	}
	if (cp - data != dict_size) {
		error("dict size mismatch");
		return NULL;
	}

	dict = xmalloc(sizeof(*dict) +
		       (nb_entries + 1) * sizeof(dict->offsets[0]));
	dict->data = data;
	dict->nb_entries = nb_entries;

	dict->offsets[0] = 0;
	cp = data;
	for (i = 0; i < nb_entries; i++) {
		cp += 2;
		cp += strlen((const char *)cp) + 1;
		dict->offsets[i + 1] = cp - data;
	}

	return dict;
}

void pv4_free_dict(struct packv4_dict *dict)
{
	if (dict) {
		free((void*)dict->data);
		free(dict);
	}
}

static struct packv4_dict *load_dict(struct packed_git *p, off_t *offset)
{
	struct pack_window *w_curs = NULL;
	off_t curpos = *offset;
	unsigned long dict_size, avail;
	unsigned char *src, *data;
	const unsigned char *cp;
	git_zstream stream;
	struct packv4_dict *dict;
	int st;

	/* get uncompressed dictionary data size */
	src = use_pack(p, &w_curs, curpos, &avail);
	cp = src;
	dict_size = decode_varint(&cp);
	curpos += cp - src;

	data = xmallocz(dict_size);
	memset(&stream, 0, sizeof(stream));
	stream.next_out = data;
	stream.avail_out = dict_size + 1;

	git_inflate_init(&stream);
	do {
		src = use_pack(p, &w_curs, curpos, &stream.avail_in);
		stream.next_in = src;
		st = git_inflate(&stream, Z_FINISH);
		curpos += stream.next_in - src;
	} while ((st == Z_OK || st == Z_BUF_ERROR) && stream.avail_out);
	git_inflate_end(&stream);
	unuse_pack(&w_curs);
	if (st != Z_STREAM_END || stream.total_out != dict_size) {
		error("pack dictionary bad");
		free(data);
		return NULL;
	}

	dict = pv4_create_dict(data, dict_size);
	if (!dict) {
		free(data);
		return NULL;
	}

	*offset = curpos;
	return dict;
}

static void load_ident_dict(struct packed_git *p)
{
	off_t offset = 12 + p->num_objects * 20;
	struct packv4_dict *names = load_dict(p, &offset);
	if (!names)
		die("bad pack name dictionary in %s", p->pack_name);
	p->ident_dict = names;
	p->ident_dict_end = offset;
}

const unsigned char *get_identref(struct packed_git *p, const unsigned char **srcp)
{
	unsigned int index;

	if (!p->ident_dict)
		load_ident_dict(p);

	index = decode_varint(srcp);
	if (index >= p->ident_dict->nb_entries) {
		error("%s: index overflow", __func__);
		return NULL;
	}
	return p->ident_dict->data + p->ident_dict->offsets[index];
}

static void load_path_dict(struct packed_git *p)
{
	off_t offset;
	struct packv4_dict *paths;

	/*
	 * For now we need to load the name dictionary to know where
	 * it ends and therefore where the path dictionary starts.
	 */
	if (!p->ident_dict)
		load_ident_dict(p);

	offset = p->ident_dict_end;
	paths = load_dict(p, &offset);
	if (!paths)
		die("bad pack path dictionary in %s", p->pack_name);
	p->path_dict = paths;
}

const unsigned char *get_pathref(struct packed_git *p, unsigned int index,
				 int *len)
{
	if (!p->path_dict)
		load_path_dict(p);

	if (index >= p->path_dict->nb_entries) {
		error("%s: index overflow", __func__);
		return NULL;
	}
	if (len)
		*len = p->path_dict->offsets[index + 1] -
			p->path_dict->offsets[index];
	return p->path_dict->data + p->path_dict->offsets[index];
}

static int tree_line(unsigned char *buf, unsigned long size,
		     const char *label, int label_len,
		     const unsigned char *sha1)
{
	static const char hex[] = "0123456789abcdef";
	int i;

	if (label_len + 1 + 40 + 1 > size)
		return 0;

	memcpy(buf, label, label_len);
	buf += label_len;
	*buf++ = ' ';

	for (i = 0; i < 20; i++) {
		unsigned int val = *sha1++;
		*buf++ = hex[val >> 4];
		*buf++ = hex[val & 0xf];
	}

	*buf = '\n';

	return label_len + 1 + 40 + 1;
}

static int ident_line(unsigned char *buf, unsigned long size,
		      const char *label, int label_len,
		      const unsigned char *ident, unsigned long time, int tz)
{
	int ident_len = strlen((const char *)ident);
	int len = label_len + 1 + ident_len + 1 + 1 + 5 + 1;
	int time_len = 0;
	unsigned char time_buf[16];

	do {
		time_buf[time_len++] = '0' + time % 10;
		time /= 10;
	} while (time);
	len += time_len;

	if (len > size)
		return 0;

	memcpy(buf, label, label_len);
	buf += label_len;
	*buf++ = ' ';

	memcpy(buf, ident, ident_len);
	buf += ident_len;
	*buf++ = ' ';

	do {
		*buf++ = time_buf[--time_len];
	} while (time_len);
	*buf++ = ' ';

	if (tz < 0) {
		tz = -tz;
		*buf++ = '-';
	} else
		*buf++ = '+';
	*buf++ = '0' + tz / 1000; tz %= 1000;
	*buf++ = '0' + tz / 100;  tz %= 100;
	*buf++ = '0' + tz / 10;   tz %= 10;
	*buf++ = '0' + tz;

	*buf = '\n';

	return len;
}

void *pv4_get_commit(struct packed_git *p, struct pack_window **w_curs,
		     off_t offset, unsigned long size)
{
	unsigned long avail;
	git_zstream stream;
	int len, st;
	unsigned int nb_parents;
	unsigned char *dst, *dcp;
	const unsigned char *src, *scp, *sha1, *ident, *author, *committer;
	unsigned long author_time, commit_time;
	int16_t author_tz, commit_tz;

	dst = xmallocz(size);
	dcp = dst;

	src = use_pack(p, w_curs, offset, &avail);
	scp = src;

	sha1 = get_sha1ref(p, &scp);
	len = tree_line(dcp, size, "tree", strlen("tree"), sha1);
	if (!len)
		die("overflow in %s", __func__);
	dcp += len;
	size -= len;

	nb_parents = decode_varint(&scp);
	while (nb_parents--) {
		sha1 = get_sha1ref(p, &scp);
		len = tree_line(dcp, size, "parent", strlen("parent"), sha1);
		if (!len)
			die("overflow in %s", __func__);
		dcp += len;
		size -= len;
	}

	commit_time = decode_varint(&scp);
	ident = get_identref(p, &scp);
	commit_tz = (ident[0] << 8) | ident[1];
	committer = &ident[2];

	author_time = decode_varint(&scp);
	ident = get_identref(p, &scp);
	author_tz = (ident[0] << 8) | ident[1];
	author = &ident[2];

	if (author_time & 1)
		author_time = commit_time + (author_time >> 1);
	else
		author_time = commit_time - (author_time >> 1);

	len = ident_line(dcp, size, "author", strlen("author"),
			 author, author_time, author_tz);
	if (!len)
		die("overflow in %s", __func__);
	dcp += len;
	size -= len;

	len = ident_line(dcp, size, "committer", strlen("committer"),
			 committer, commit_time, commit_tz);
	if (!len)
		die("overflow in %s", __func__);
	dcp += len;
	size -= len;

	if (scp - src > avail)
		die("overflow in %s", __func__);
	offset += scp - src;

	memset(&stream, 0, sizeof(stream));
	stream.next_out = dcp;
	stream.avail_out = size + 1;
	git_inflate_init(&stream);
	do {
		src = use_pack(p, w_curs, offset, &stream.avail_in);
		stream.next_in = (unsigned char *)src;
		st = git_inflate(&stream, Z_FINISH);
		offset += stream.next_in - src;
	} while ((st == Z_OK || st == Z_BUF_ERROR) && stream.avail_out);
	git_inflate_end(&stream);
	if (st != Z_STREAM_END || stream.total_out != size) {
		free(dst);
		return NULL;
	}

	return dst;
}

static int copy_canonical_tree_entries(struct packed_git *p, off_t offset,
				       unsigned int start, unsigned int count,
				       unsigned char **dstp, unsigned long *sizep)
{
	void *data;
	const unsigned char *from, *end;
	enum object_type type;
	unsigned long size;
	struct tree_desc desc;

	data = unpack_entry(p, offset, &type, &size);
	if (!data)
		return -1;
	if (type != OBJ_TREE) {
		free(data);
		return -1;
	}

	init_tree_desc(&desc, data, size);

	while (start--)
		update_tree_entry(&desc);

	from = desc.buffer;
	while (count--)
		update_tree_entry(&desc);
	end = desc.buffer;

	if (end - from > *sizep) {
		free(data);
		return -1;
	}
	memcpy(*dstp, from, end - from);
	*dstp += end - from;
	*sizep -= end - from;
	free(data);
	return 0;
}

/* ordering is so that member alignment takes the least amount of space */
struct pv4_tree_cache {
	off_t base_offset;
	off_t offset;
	off_t last_copy_base;
	struct packed_git *p;
	unsigned int pos;
	unsigned int nb_entries;
};

#define CACHE_SIZE 1024
static struct pv4_tree_cache pv4_tree_cache[CACHE_SIZE];

static struct pv4_tree_cache *get_tree_offset_cache(struct packed_git *p,
						    off_t base_offset,
						    struct pv4_tree_cache *c)
{
	unsigned long hash;

	if (c && c->p == p && c->base_offset == base_offset)
		return c;

	hash = (unsigned long)p + (unsigned long)base_offset;
	hash += (hash >> 8) + (hash >> 16);
	hash %= CACHE_SIZE;

	c = &pv4_tree_cache[hash];
	if (c->p != p || c->base_offset != base_offset) {
		c->p = p;
		c->base_offset = base_offset;
		c->offset = 0;
		c->last_copy_base = 0;
		c->pos = 0;
		c->nb_entries = 0;
	}
	return c;
}

static int tree_entry_prefix(unsigned char *buf, unsigned long size,
			     const unsigned char *path, int path_len,
			     unsigned mode)
{
	int mode_len = 0;
	int len;
	unsigned char mode_buf[8];

	do {
		mode_buf[mode_len++] = '0' + (mode & 7);
		mode >>= 3;
	} while (mode);

	len = mode_len + 1 + path_len;
	if (len > size)
		return 0;

	do {
		*buf++ = mode_buf[--mode_len];
	} while (mode_len);
	*buf++ = ' ';
	memcpy(buf, path, path_len);

	return len;
}

static int generate_tree_entry(struct packed_git *p,
			       const unsigned char **bufp,
			       unsigned char **dstp, unsigned long *sizep,
			       int what)
{
	const unsigned char *path, *sha1;
	unsigned mode;
	int len, pathlen;

	path = get_pathref(p, what >> 1, &pathlen);
	sha1 = get_sha1ref(p, bufp);
	if (!path || !sha1)
		return -1;
	mode = (path[0] << 8) | path[1];
	len = tree_entry_prefix(*dstp, *sizep,
				path + 2, pathlen - 2, mode);
	if (!len || len + 20 > *sizep)
		return -1;
	hashcpy(*dstp + len, sha1);
	len += 20;
	*dstp += len;
	*sizep -= len;
	return 0;
}

static int decode_entries(struct packed_git *p, struct pack_window **w_curs,
			  off_t obj_offset, unsigned int start, unsigned int count,
			  unsigned char **dstp, unsigned long *sizep)
{
	unsigned long avail;
	const unsigned char *src, *scp;
	unsigned int curpos;
	off_t offset, copy_objoffset;
	struct pv4_tree_cache *c;

	c = get_tree_offset_cache(p, obj_offset, NULL);
	if (count && start < c->nb_entries && start >= c->pos &&
	    count <= c->nb_entries - start) {
		offset = c->offset;
		copy_objoffset = c->last_copy_base;
		curpos = c->pos;
		start -= curpos;
		src = NULL;
		avail = 0;
	} else {
		unsigned int nb_entries;

		src = use_pack(p, w_curs, obj_offset, &avail);
		scp = src;

		/* we need to skip over the object header */
		while (*scp & 128)
			if (++scp - src >= avail - 20)
				return -1;

		switch (*scp++ & 0xf) {
		/* is this a canonical tree object? */
		case OBJ_TREE:
		case OBJ_REF_DELTA:
			return copy_canonical_tree_entries(p, obj_offset,
							   start, count,
							   dstp, sizep);
		/* let's still make sure this is actually a pv4 tree */
		case OBJ_PV4_TREE:
			break;
		default:
			return -1;
		}

		nb_entries = decode_varint(&scp);
		if (!count)
			count = nb_entries;
		if (!nb_entries || start > nb_entries ||
		    count > nb_entries - start)
			return -1;

		curpos = 0;
		copy_objoffset = 0;
		offset = obj_offset + (scp - src);
		avail -= scp - src;
		src = scp;

		/*
		 * If this is a partial copy, let's (re)initialize a cache
		 * entry to speed things up if the remaining of this tree
		 * is needed in the future.
		 */
		if (start + count < nb_entries) {
			c->offset = offset;
			c->pos = 0;
			c->nb_entries = nb_entries;
			c->last_copy_base = 0;
		}
	}

	while (count) {
		unsigned int what;

		if (avail < 20) {
			src = use_pack(p, w_curs, offset, &avail);
			if (avail < 20)
				return -1;
		}
		scp = src;

		what = decode_varint(&scp);
		if (scp == src)
			return -1;

		if (!(what & 1) && start != 0) {
			/*
			 * This is a single entry and we have to skip it.
			 * The path index was parsed and is in 'what'.
			 * Skip over the SHA1 index.
			 */
			if (!*scp)
				scp += 1 + 20;
			else
				while (*scp++ & 128);
			start--;
			curpos++;
		} else if (!(what & 1) && start == 0) {
			/*
			 * This is an actual tree entry to recreate.
			 */
			if (generate_tree_entry(p, &scp, dstp, sizep, what))
				return -1;
			count--;
			curpos++;
		} else if (what & 1) {
			/*
			 * Copy from another tree object.
			 */
			unsigned int copy_start, copy_count;

			copy_start = what >> 1;
			copy_count = decode_varint(&scp);
			if (!copy_count)
				return -1;

			/*
			 * The LSB of copy_count is a flag indicating if
			 * a third value is provided to specify the source
			 * object.  This may be omitted when it doesn't
			 * change, but has to be specified at least for the
			 * first copy sequence.
			 */
			if (copy_count & 1) {
				unsigned index = decode_varint(&scp);
				if (!index) {
					/*
					 * SHA1 follows. We assume the
					 * object is in the same pack.
					 */
					copy_objoffset =
						find_pack_entry_one(scp, p);
					scp += 20;
				} else {
					/*
					 * From the SHA1 index we can get
					 * the object offset directly.
					 */
					copy_objoffset =
						nth_packed_object_offset(p, index - 1);
				}
			}
			copy_count >>= 1;
			if (!copy_count || !copy_objoffset)
				return -1;

			if (start >= copy_count) {
				start -= copy_count;
				curpos += copy_count;
			} else {
				int ret;

				copy_count -= start;
				copy_start += start;
				if (copy_count > count) {
					/*
					 * We won't consume the whole of
					 * this copy sequence and the main
					 * loop will be exited. Let's manage
					 * for offset and curpos to remain
					 * unchanged to update the cache.
					 */
					copy_count = count;
					count = 0;
					scp = src;
				} else {
					count -= copy_count;
					curpos += start + copy_count;
					start = 0;
				}

				ret = decode_entries(p, w_curs, copy_objoffset,
						     copy_start, copy_count,
						     dstp, sizep);
				if (ret)
					return ret;

				/* force pack window readjustment */
				avail = scp - src;
			}
		}

		offset += scp - src;
		avail -= scp - src;
		src = scp;
	}

	/*
	 * Update the cache if we didn't run through the entire tree.
	 * We have to "get" it again as a recursion into decode_entries()
	 * could have invalidated what we obtained initially.
	 */
	c = get_tree_offset_cache(p, obj_offset, c);
	if (curpos < c->nb_entries) {
		c->pos = curpos;
		c->offset = offset;
		c->last_copy_base = copy_objoffset;
	}

	return 0;
}

static void *alt_get_tree(struct packed_git *p, struct pack_window **w_curs,
			  off_t obj_offset, unsigned long size)
{
	struct decode_state ds, *dsp;
	unsigned char *dst, *dcp;
	int ret;

	dst = xmallocz(size);
	dcp = dst;
	memset(&ds, 0, sizeof(ds));
	ds.p = p;
	ds.w_curs = w_curs;
	ds.obj_offset = obj_offset;
	dsp = &ds;
	while (1) {
		ret = decode_tree_entry(&dsp);
		if (ret == decode_failed)
			break;
		if (ret == decode_done) {
			if (size != 0)
				break;
			return dst;
		}
		if (dsp->state == fallingback) {
			const unsigned char *start = dsp->desc.buffer;
			const unsigned char *end = dsp->desc.entry.sha1 + 20;
			memcpy(dcp, start, end - start);
			dcp += end - start;
			size -= end - start;
		} else {
			int len, pathlen;
			const unsigned char *path;
			unsigned mode;

			path = get_pathref(dsp->p, dsp->path_index, &pathlen);
			if (!path)
				break;
			mode = (path[0] << 8) | path[1];

			len = tree_entry_prefix(dcp, size, path + 2,
						pathlen - 2, mode);
			if (!len || len + 20 > size)
				break;
			hashcpy(dcp + len, dsp->sha1);
			len  += 20;
			dcp  += len;
			size -= len;
		}
	}
	free(dst);
	return NULL;
}

void *pv4_get_tree(struct packed_git *p, struct pack_window **w_curs,
		   off_t obj_offset, unsigned long size)
{
	static int use_decode_tree_entry = -1;
	unsigned char *dst, *dcp;
	int ret;

	if (use_decode_tree_entry > 0 ||
	    (use_decode_tree_entry == -1 &&
	     (use_decode_tree_entry = getenv("DECODE_TREE_ENTRY") != NULL)))
		return alt_get_tree(p, w_curs, obj_offset, size);

	dst = xmallocz(size);
	dcp = dst;
	ret = decode_entries(p, w_curs, obj_offset, 0, 0, &dcp, &size);
	if (ret < 0 || size != 0) {
		free(dst);
		return NULL;
	}
	return dst;
}

unsigned long pv4_unpack_object_header_buffer(const unsigned char *base,
					      unsigned long len,
					      enum object_type *type,
					      unsigned long *sizep)
{
	const unsigned char *cp = base;
	uintmax_t val = decode_varint(&cp);
	*type = val & 0xf;
	*sizep = val >> 4;
	return cp - base;
}

/*
 * xmalloc() shows up even higher than decode_tree_entry() in perf
 * report. Try to avoid it.
 */
static struct decode_state *free_decode_states;

static struct decode_state* new_decode_state(void)
{
	struct decode_state *ds;
	if (free_decode_states) {
		ds = free_decode_states;
		free_decode_states = ds->free;
	} else
		ds = xmalloc(sizeof(*ds));
	ds->free = NULL;
	return ds;
}

static void free_decode_state(struct decode_state *ds)
{
	assert(ds->free == NULL);
	ds->free = free_decode_states;
	free_decode_states = ds;
}

/*
 * Input state: preparing
 * Output state: fallingback
 * ds->desc is made ready for decode_canonical() to use
 */
static inline int prepare_canonical_tree(struct decode_state *ds)
{
	void *data;
	enum object_type type;
	unsigned long size;

	data = unpack_entry(ds->p, ds->obj_offset, &type, &size);
	if (!data)
		return error("fail to unpack entry at offset %"PRIuMAX, ds->obj_offset);
	if (type != OBJ_TREE) {
		free(data);
		return error("entry at offset %"PRIuMAX" is not a tree", ds->obj_offset);
	}

	if (ds->count == 0)
		ds->count = 0xffffffff; /* magic number.. */
	init_tree_desc(&ds->desc, data, size);
	ds->tree_v2 = data;
	ds->state = fallingback;

	while (ds->skip--)
		update_tree_entry(&ds->desc);

	return decode_zero;
}

/*
 * Input state: preparing
 * Output state: decoding or fallingback
 * Packv4 data section is made ready for decode_entry() to use
 */
static inline int prepare_for_decoding(struct decode_state *ds)
{
	struct pv4_tree_cache *c;
	const unsigned char *src;
	const unsigned char *scp;
	unsigned int nb_entries;
	unsigned long avail;

	ds->cache = c = get_tree_offset_cache(ds->p, ds->obj_offset, NULL);
	if (0      <  ds->count && ds->count <= c->nb_entries - ds->skip &&
	    c->pos <= ds->skip && ds->skip <  c->nb_entries) {
		ds->state   = decoding;
		ds->offset  = c->offset;
		ds->curpos  = c->pos;
		ds->skip   -= c->pos;
		ds->src	    = NULL;
		ds->avail   = 0;
		ds->nb_entries = c->nb_entries;
		ds->last_copy_base = c->last_copy_base;
		return decode_zero;
	}

	src = use_pack(ds->p, ds->w_curs, ds->obj_offset, &avail);
	scp = src;

	/* we need to skip over the object header */
	while (*scp & 128)
		if (++scp - src >= avail - 20)
			return error("not enough data at line %d", __LINE__);

	switch (*scp++ & 0xf) {
	case OBJ_TREE:
	case OBJ_REF_DELTA:
	case OBJ_OFS_DELTA:
		return prepare_canonical_tree(ds);
	case OBJ_PV4_TREE:
		break;
	default:
		return error("invalid object type %d", scp[-1] & 0xf);
	}

	assert((scp[-1] & 0xf) == OBJ_PV4_TREE);
	nb_entries = decode_varint(&scp);
	if (!ds->count)
		ds->count = nb_entries;
	if (!nb_entries || ds->skip > nb_entries ||
	    ds->count > nb_entries - ds->skip)
		return error("ds->skip or ds->count is invalid");

	ds->state = decoding;
	ds->nb_entries = nb_entries;
	ds->curpos = 0;
	ds->last_copy_base = 0;
	ds->offset = ds->obj_offset + (scp - src);
	ds->avail = avail - (scp - src);
	ds->src = scp;

	/*
	 * If this is a partial copy, let's (re)initialize a cache
	 * entry to speed things up if the remaining of this tree is
	 * needed in the future.
	 */
	if (ds->skip + ds->count < nb_entries) {
		c->pos = ds->curpos;
		c->offset = ds->offset;
		c->nb_entries = ds->nb_entries;
		c->last_copy_base = ds->last_copy_base;
	}
	return decode_zero;
}

/*
 * Prepare to decode_tree to copy from another tree. A new
 * decode_state may be allocated and linked to the current one.
 */
static inline int switch_tree(struct decode_state **dsp,
			      unsigned int copy_start,
			      const unsigned char *scp)
{
	struct decode_state *ds = *dsp;
	struct decode_state *sub_ds;
	unsigned int copy_count;

	copy_count = decode_varint(&scp);
	if (!copy_count)
		return error("copy_count is zero");

	/*
	 * The LSB of copy_count is a flag indicating if a third value
	 * is provided to specify the source object.  This may be
	 * omitted when it doesn't change, but has to be specified at
	 * least for the first copy sequence.
	 */
	if (copy_count & 1) {
		unsigned index = decode_varint(&scp);
		if (!index) {
			/*
			 * SHA1 follows. We assume the object is in
			 * the same pack.
			 */
			ds->last_copy_base =
				find_pack_entry_one(scp, ds->p);
			scp += 20;
		} else {
			/*
			 * From the SHA1 index we can get the object
			 * offset directly.
			 */
			ds->last_copy_base =
				nth_packed_object_offset(ds->p, index - 1);
		}
	}
	copy_count >>= 1;
	if (!copy_count || !ds->last_copy_base)
		return error("copy_count or copy_objoffset is zero");

	if (ds->skip >= copy_count) {
		ds->skip   -= copy_count;
		ds->curpos += copy_count;
		ds->offset += scp - ds->src;
		ds->avail  -= scp - ds->src;
		ds->src     = scp;
		return decode_zero;
	}

	copy_count -= ds->skip;
	copy_start += ds->skip;

	if (copy_count > ds->count) {
		/*
		 * We won't consume the whole of this copy sequence
		 * and the main loop will be exited. Let's manage for
		 * offset and curpos to remain unchanged to update the
		 * cache.
		 */
		copy_count = ds->count;
		ds->count = 0;
	} else {
		ds->count -= copy_count;
		ds->curpos += ds->skip + copy_count;
		ds->skip = 0;
		ds->offset += scp - ds->src;
		ds->src     = scp;
	}
	ds->avail = 0;	/* force pack window readjustment */

	sub_ds	       = new_decode_state();
	sub_ds->up     = ds;
	sub_ds->p      = ds->p;
	sub_ds->w_curs = ds->w_curs;
	sub_ds->obj_offset = ds->last_copy_base;
	sub_ds->skip   = copy_start;
	sub_ds->count  = copy_count;
	sub_ds->state  = preparing;
	/* the rest of sub_ds is initialized in prepare_for_decoding() */

	*dsp = sub_ds;
	return decode_zero;
}

/*
 * Prepare data in (*dsp)->desc.entry.
 */
static inline int decode_canonical(struct decode_state **dsp)
{
	struct decode_state *ds = *dsp;

	if ((ds->count == 0xffffffff && ds->desc.size == 0) ||
	     ds->count == 0) {
		struct decode_state *sub_ds;

		free(ds->tree_v2);
		if (!ds->up)
			return decode_done;
		sub_ds = ds;
		ds = ds->up;
		*dsp = ds;
		free_decode_state(sub_ds);
		return decode_zero;
	}
	if (ds->count != 0xffffffff)
		ds->count--;
	update_tree_entry(&ds->desc);
	return decode_one;
}

static inline int do_decode_tree_entry(struct decode_state **dsp)
{
	struct decode_state *ds = *dsp;
	const unsigned char *scp;
	unsigned int what;

	switch (ds->state) {
	case preparing:
		return prepare_for_decoding(ds);
	case fallingback:
		return decode_canonical(dsp);
	case decoding:
		break;
	default:
		return error("invalid decode state %d", ds->state);
	}

	if (ds->count == 0) {
		struct decode_state *sub_ds;
		/*
		 * Update the cache if we didn't run through the
		 * entire tree.  We have to "get" it again because we
		 * may have invalidated it when we switched trees.
		 */
		struct pv4_tree_cache *c =
			get_tree_offset_cache(ds->p, ds->obj_offset,
					      ds->cache);
		if (ds->curpos < c->nb_entries) {
			c->pos = ds->curpos;
			c->offset = ds->offset;
			c->last_copy_base = ds->last_copy_base;
		}

		if (!ds->up)
			return decode_done;

		sub_ds = ds;
		ds = ds->up;
		*dsp = ds;
		free_decode_state(sub_ds);
		return decode_zero;
	}

	if (ds->avail < 20) {
		ds->src = use_pack(ds->p, ds->w_curs, ds->offset, &ds->avail);
		if (ds->avail < 20)
			return error("truncated pack");
	}

	scp = ds->src;
	what = decode_varint(&scp);
	if (scp == ds->src)
		return error("failed to decode");

	if (what & 1)
		return switch_tree(dsp, what >> 1, scp);

	if (ds->skip) {
		if (*scp) {
			while (*scp++ & 128)
				; /* nothing */
		}
		else
			scp += 1 + 20;
		ds->skip--;
		ds->curpos++;
		ds->offset += scp - ds->src;
		ds->avail  -= scp - ds->src;
		ds->src     = scp;
		return decode_zero;
	}

	assert(ds->skip == 0 && ds->count != 0);
	ds->path_index = what >> 1;
	ds->sha1 = get_sha1ref(ds->p, &scp);
	if (!ds->sha1)
		return error("invalid tree SHA-1");

	ds->count--;
	ds->curpos++;
	ds->offset += scp - ds->src;
	ds->avail  -= scp - ds->src;
	ds->src     = scp;
	return decode_one;
}

/*
 * Free all decode_state allocated locally. Return the top
 * decode_state, which may or may not be free()'d
 */
static struct decode_state *free_sub_decode_states(struct decode_state *ds)
{
	struct decode_state *ret;
	if (ds->state == fallingback)
		free(ds->tree_v2);
	if (!ds->up)
		return ds;
	ret = free_sub_decode_states(ds->up);
	free(ds);
	return ret;
}

/*
 * 'p', 'w_curs', 'obj_offset', 'skip' and 'count' must be set in
 * *dsp. count == 0 means get all remaining tree entries.
 * decode_entry() returns
 *
 *  - decode_one: if 'state' is decoding, raw v4 tree entry is in
 *      sha1_index and path_index. If 'state' is fallingback, v2 tree
 *      is in desc.entry.
 *
 *  - decode_done:   self explanatory
 *
 *  - decode_failed: self explanatory
 */
int decode_tree_entry(struct decode_state **dsp)
{
	int ret;
	while ((ret = do_decode_tree_entry(dsp)) == decode_zero)
		;		/* continue */
	if (ret == decode_failed)
		*dsp = free_sub_decode_states(*dsp);
	return ret;
}
