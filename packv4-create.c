/*
 * packv4-create.c: creation of dictionary tables and objects used in pack v4
 *
 * (C) Nicolas Pitre <nico@fluxnic.net>
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "cache.h"
#include "object.h"
#include "tree-walk.h"
#include "pack.h"
#include "pack-revindex.h"
#include "progress.h"


static int pack_compression_seen;
static int pack_compression_level = Z_DEFAULT_COMPRESSION;

struct data_entry {
	unsigned offset;
	unsigned size;
	unsigned hits;
};

struct dict_table {
	unsigned char *data;
	unsigned ptr;
	unsigned size;
	struct data_entry *entry;
	unsigned nb_entries;
	unsigned max_entries;
	unsigned *hash;
	unsigned hash_size;
};

struct dict_table *create_dict_table(void)
{
	return xcalloc(sizeof(struct dict_table), 1);
}

void destroy_dict_table(struct dict_table *t)
{
	free(t->data);
	free(t->entry);
	free(t->hash);
	free(t);
}

static int locate_entry(struct dict_table *t, const void *data, int size)
{
	int i = 0, len = size;
	const unsigned char *p = data;

	while (len--)
		i = i * 111 + *p++;
	i = (unsigned)i % t->hash_size;

	while (t->hash[i]) {
		unsigned n = t->hash[i] - 1;
		if (t->entry[n].size == size &&
		    memcmp(t->data + t->entry[n].offset, data, size) == 0)
			return n;
		if (++i >= t->hash_size)
			i = 0;
	}
	return -1 - i;
}

static void rehash_entries(struct dict_table *t)
{
	unsigned n;

	t->hash_size *= 2;
	if (t->hash_size < 1024)
		t->hash_size = 1024;
	t->hash = xrealloc(t->hash, t->hash_size * sizeof(*t->hash));
	memset(t->hash, 0, t->hash_size * sizeof(*t->hash));

	for (n = 0; n < t->nb_entries; n++) {
		int i = locate_entry(t, t->data + t->entry[n].offset,
					t->entry[n].size);
		if (i < 0)
			t->hash[-1 - i] = n + 1;
	}
}

int dict_add_entry(struct dict_table *t, int val, const char *str, int str_len)
{
	int i, val_len = 2;

	if (t->ptr + val_len + str_len > t->size) {
		t->size = (t->size + val_len + str_len + 1024) * 3 / 2;
		t->data = xrealloc(t->data, t->size);
	}

	t->data[t->ptr] = val >> 8;
	t->data[t->ptr + 1] = val;
	memcpy(t->data + t->ptr + val_len, str, str_len);
	t->data[t->ptr + val_len + str_len] = 0;

	i = (t->nb_entries) ?
		locate_entry(t, t->data + t->ptr, val_len + str_len) : -1;
	if (i >= 0) {
		t->entry[i].hits++;
		return i;
	}

	if (t->nb_entries >= t->max_entries) {
		t->max_entries = (t->max_entries + 1024) * 3 / 2;
		t->entry = xrealloc(t->entry, t->max_entries * sizeof(*t->entry));
	}
	t->entry[t->nb_entries].offset = t->ptr;
	t->entry[t->nb_entries].size = val_len + str_len;
	t->entry[t->nb_entries].hits = 1;
	t->ptr += val_len + str_len + 1;
	t->nb_entries++;

	if (t->hash_size * 3 <= t->nb_entries * 4)
		rehash_entries(t);
	else
		t->hash[-1 - i] = t->nb_entries;

	return t->nb_entries - 1;
}

static int cmp_dict_entries(const void *a_, const void *b_)
{
	const struct data_entry *a = a_;
	const struct data_entry *b = b_;
	int diff = b->hits - a->hits;
	if (!diff)
		diff = a->offset - b->offset;
	return diff;
}

static void sort_dict_entries_by_hits(struct dict_table *t)
{
	qsort(t->entry, t->nb_entries, sizeof(*t->entry), cmp_dict_entries);
	t->hash_size = (t->nb_entries * 4 / 3) / 2;
	rehash_entries(t);
}

static struct dict_table *commit_name_table;
static struct dict_table *tree_path_table;

/*
 * Parse the author/committer line from a canonical commit object.
 * The 'from' argument points right after the "author " or "committer "
 * string.  The time zone is parsed and stored in *tz_val.  The returned
 * pointer is right after the end of the email address which is also just
 * before the time value, or NULL if a parsing error is encountered.
 */
static char *get_nameend_and_tz(char *from, int *tz_val)
{
	char *end, *tz;

	tz = strchr(from, '\n');
	/* let's assume the smallest possible string to be " <> 0 +0000\n" */
	if (!tz || tz - from < 11)
		return NULL;
	tz -= 4;
	end = tz - 4;
	while (end - from > 3 && *end != ' ')
		end--;
	if (end[-1] != '>' || end[0] != ' ' || tz[-2] != ' ')
		return NULL;
	*tz_val = (tz[0] - '0') * 1000 +
		  (tz[1] - '0') * 100 +
		  (tz[2] - '0') * 10 +
		  (tz[3] - '0');
	switch (tz[-1]) {
	default:	return NULL;
	case '+':	break;
	case '-':	*tz_val = -*tz_val;
	}
	return end;
}

static int add_commit_dict_entries(void *buf, unsigned long size)
{
	char *name, *end = NULL;
	int tz_val;

	if (!commit_name_table)
		commit_name_table = create_dict_table();

	/* parse and add author info */
	name = strstr(buf, "\nauthor ");
	if (name) {
		name += 8;
		end = get_nameend_and_tz(name, &tz_val);
	}
	if (!name || !end)
		return -1;
	dict_add_entry(commit_name_table, tz_val, name, end - name);

	/* parse and add committer info */
	name = strstr(end, "\ncommitter ");
	if (name) {
	       name += 11;
	       end = get_nameend_and_tz(name, &tz_val);
	}
	if (!name || !end)
		return -1;
	dict_add_entry(commit_name_table, tz_val, name, end - name);

	return 0;
}

static int add_tree_dict_entries(void *buf, unsigned long size)
{
	struct tree_desc desc;
	struct name_entry name_entry;

	if (!tree_path_table)
		tree_path_table = create_dict_table();

	init_tree_desc(&desc, buf, size);
	while (tree_entry(&desc, &name_entry)) {
		int pathlen = tree_entry_len(&name_entry);
		dict_add_entry(tree_path_table, name_entry.mode,
				name_entry.path, pathlen);
	}

	return 0;
}

void dump_dict_table(struct dict_table *t)
{
	int i;

	sort_dict_entries_by_hits(t);
	for (i = 0; i < t->nb_entries; i++) {
		int16_t val;
		uint16_t uval;
		val = t->data[t->entry[i].offset] << 8;
		val |= t->data[t->entry[i].offset + 1];
		uval = val;
		printf("%d\t%d\t%o\t%s\n",
			t->entry[i].hits, val, uval,
			t->data + t->entry[i].offset + 2);
	}
}

static void dict_dump(void)
{
	dump_dict_table(commit_name_table);
	dump_dict_table(tree_path_table);
}

/*
 * Encode a numerical value with variable length into the destination buffer
 */
static unsigned char *add_number(unsigned char *dst, uint64_t value)
{
	unsigned char buf[20];
	unsigned pos = sizeof(buf) - 1;
	buf[pos] = value & 127;
	while (value >>= 7)
		buf[--pos] = 128 | (--value & 127);
	do {
		*dst++ = buf[pos++];
	} while (pos < sizeof(buf));
	return dst;
}

/*
 * Encode an object SHA1 reference with either an object index into the
 * pack SHA1 table incremented by 1, or the literal SHA1 value prefixed
 * with a zero byte if the needed SHA1 is not available in the table.
 */
static struct pack_idx_entry *all_objs;
static unsigned all_objs_nr;
static unsigned char *add_sha1_ref(unsigned char *dst, const unsigned char *sha1)
{
	unsigned lo = 0, hi = all_objs_nr;

	do {
		unsigned mi = (lo + hi) / 2;
		int cmp = hashcmp(all_objs[mi].sha1, sha1);

		if (cmp == 0)
			return add_number(dst, mi + 1);
		if (cmp > 0)
			hi = mi;
		else
			lo = mi+1;
	} while (lo < hi);

	*dst++ = 0;
	hashcpy(dst, sha1);
	return dst + 20;
}

/*
 * This converts a canonical commit object buffer into its
 * tightly packed representation using the already populated
 * and sorted commit_name_table dictionary.  The parsing is
 * strict so to ensure the canonical version may always be
 * regenerated and produce the same hash.
 */
void * conv_to_dict_commit(void *buffer, unsigned long *psize)
{
	unsigned long size = *psize;
	char *in, *tail, *end;
	unsigned char *out;
	unsigned char sha1[20];
	int nb_parents, index, tz_val;
	unsigned long time;
	z_stream stream;
	int status;

	/*
	 * It is guaranteed that the output is always going to be smaller
	 * than the input.  We could even do this conversion in place.
	 */
	in = buffer;
	tail = in + size;
	buffer = xmalloc(size);
	out = buffer;

	/* parse the "tree" line */
	if (in + 46 >= tail || memcmp(in, "tree ", 5) || in[45] != '\n')
		goto bad_data;
	if (get_sha1_hex(in + 5, sha1) < 0)
		goto bad_data;
	in += 46;
	out = add_sha1_ref(out, sha1);

	/* count how many "parent" lines */
	nb_parents = 0;
	while (in + 48 < tail && !memcmp(in, "parent ", 7) && in[47] == '\n') {
		nb_parents++;
		in += 48;
	}
	out = add_number(out, nb_parents);

	/* rewind and parse the "parent" lines */
	in -= 48 * nb_parents;
	while (nb_parents--) {
		if (get_sha1_hex(in + 7, sha1))
			goto bad_data;
		out = add_sha1_ref(out, sha1);
		in += 48;
	}

	/* parse the "author" line */
	/* it must be at least "author x <x> 0 +0000\n" i.e. 21 chars */
	if (in + 21 >= tail || memcmp(in, "author ", 7))
		goto bad_data;
	in += 7;
	end = get_nameend_and_tz(in, &tz_val);
	if (!end)
		goto bad_data;
	index = dict_add_entry(commit_name_table, tz_val, in, end - in);
	if (index < 0)
		goto bad_dict;
	out = add_number(out, index);
	time = strtoul(end, &end, 10);
	if (!end || end[0] != ' ' || end[6] != '\n')
		goto bad_data;
	out = add_number(out, time);
	in = end + 7;

	/* parse the "committer" line */
	/* it must be at least "committer x <x> 0 +0000\n" i.e. 24 chars */
	if (in + 24 >= tail || memcmp(in, "committer ", 7))
		goto bad_data;
	in += 10;
	end = get_nameend_and_tz(in, &tz_val);
	if (!end)
		goto bad_data;
	index = dict_add_entry(commit_name_table, tz_val, in, end - in);
	if (index < 0)
		goto bad_dict;
	out = add_number(out, index);
	time = strtoul(end, &end, 10);
	if (!end || end[0] != ' ' || end[6] != '\n')
		goto bad_data;
	out = add_number(out, time);
	in = end + 7;

	/* finally, deflate the remaining data */
	memset(&stream, 0, sizeof(stream));
	deflateInit(&stream, pack_compression_level);
	stream.next_in = (unsigned char *)in;
	stream.avail_in = tail - in;
	stream.next_out = (unsigned char *)out;
	stream.avail_out = size - (out - (unsigned char *)buffer);
	status = deflate(&stream, Z_FINISH);
	end = (char *)stream.next_out;
	deflateEnd(&stream);
	if (status != Z_STREAM_END) {
		error("deflate error status %d", status);
		goto bad;
	}

	*psize = end - (char *)buffer;
	return buffer;

bad_data:
	error("bad commit data");
	goto bad;
bad_dict:
	error("bad dict entry");
bad:
	free(buffer);
	return NULL;
}

static int compare_tree_entries(struct name_entry *e1, struct name_entry *e2)
{
	int len1 = tree_entry_len(e1);
	int len2 = tree_entry_len(e2);
	int len = len1 < len2 ? len1 : len2;
	unsigned char c1, c2;
	int cmp;

	cmp = memcmp(e1->path, e2->path, len);
	if (cmp)
		return cmp;
	c1 = e1->path[len];
	c2 = e2->path[len];
	if (!c1 && S_ISDIR(e1->mode))
		c1 = '/';
	if (!c2 && S_ISDIR(e2->mode))
		c2 = '/';
	return c1 - c2;
}

/*
 * This converts a canonical tree object buffer into its
 * tightly packed representation using the already populated
 * and sorted tree_path_table dictionary.  The parsing is
 * strict so to ensure the canonical version may always be
 * regenerated and produce the same hash.
 *
 * If a delta buffer is provided, we may encode multiple ranges of tree
 * entries against that buffer.
 */
void * conv_to_dict_tree(void *buffer, unsigned long *psize,
			 void *delta, unsigned long delta_size,
			 const unsigned char *delta_sha1)
{
	unsigned long size = *psize;
	unsigned char *in, *out, *end;
	struct tree_desc desc, delta_desc;
	struct name_entry name_entry, delta_entry;
	int nb_entries;
	unsigned int delta_start, delta_pos = 0, delta_match = 0, first_delta = 1;

	if (!size)
		return NULL;

	if (!delta_size)
		delta = NULL;

	/*
	 * We can't make sure the result will always be smaller.
	 * The smallest possible entry is "0 x\0<40 byte SHA1>"
	 * or 44 bytes.  The output entry may have a realistic path index
	 * encoding using up to 3 bytes, and a non indexable SHA1 meaning
	 * 41 bytes.  And the output data already has the and nb_entries
	 * headers.  In practice the output size will be significantly
	 * smaller but for now let's make it simple.
	 */
	in = buffer;
	out = xmalloc(size);
	end = out + size;
	buffer = out;

	/* let's count how many entries there are */
	init_tree_desc(&desc, in, size);
	nb_entries = 0;
	while (tree_entry(&desc, &name_entry))
		nb_entries++;
	out = add_number(out, nb_entries);

	init_tree_desc(&desc, in, size);
	if (delta) {
		init_tree_desc(&delta_desc, delta, delta_size);
		if (!tree_entry(&delta_desc, &delta_entry))
			delta = NULL;
	}

	while (tree_entry(&desc, &name_entry)) {
		int pathlen, index;

		/*
		 * Try to match entries against our delta object.
		 */
		if (delta) {
			int ret;

			do {
				ret = compare_tree_entries(&name_entry, &delta_entry);
				if (ret < 1)
					break;
				if (tree_entry(&delta_desc, &delta_entry))
					delta_pos++;
				else
					delta = NULL;
			} while (delta);

			if (ret == 0) {
				if (!delta_match)
					delta_start = delta_pos;
				delta_match++;
				continue;
			}
		}

		if (delta_match) {
			/*
			 * Let's write a sequence indicating we're copying
			 * entries from another object:
			 *
			 * 0 + start_entry + nr_entries + object_ref
			 *
			 * However, if object_ref is the same as the
			 * preceeding one, we can omit it and save some
			 * more space, especially if that ends up being a
			 * full sha1 reference.  Let's steal the LSB
			 * of delta_start for that purpose.
			 */
			if (end - out < (first_delta ? 48 : 7)) {
				unsigned long sofar = out - (unsigned char *)buffer;
				buffer = xrealloc(buffer, sofar + 48);
				out = (unsigned char *)buffer + sofar;
				end = out + 48;
			}

			delta_start <<= 1;
			delta_start |= first_delta;
			out = add_number(out, 0);
			out = add_number(out, delta_start);
			out = add_number(out, delta_match);
			if (first_delta)
				out = add_sha1_ref(out, delta_sha1);
			delta_match = 0;
			first_delta = 0;
		}

		if (end - out < 45) {
			unsigned long sofar = out - (unsigned char *)buffer;
			buffer = xrealloc(buffer, sofar + 45);
			out = (unsigned char *)buffer + sofar;
			end = out + 45;
		}

		pathlen = tree_entry_len(&name_entry);
		index = dict_add_entry(tree_path_table, name_entry.mode,
				       name_entry.path, pathlen);
		if (index < 0) {
			error("missing tree dict entry");
			free(buffer);
			return NULL;
		}
		out = add_number(out, index);
		out = add_sha1_ref(out, name_entry.sha1);
	}

	*psize = out - (unsigned char *)buffer;
	return buffer;
}

static struct pack_idx_entry *get_packed_object_list(struct packed_git *p)
{
	unsigned i, nr_objects = p->num_objects;
	struct pack_idx_entry *objects;

	objects = xmalloc((nr_objects + 1) * sizeof(*objects));
	objects[nr_objects].offset = p->pack_size - 20;
	for (i = 0; i < nr_objects; i++) {
		hashcpy(objects[i].sha1, nth_packed_object_sha1(p, i));
		objects[i].offset = nth_packed_object_offset(p, i);
	}

	return objects;
}

static int sort_by_offset(const void *e1, const void *e2)
{
	const struct pack_idx_entry * const *entry1 = e1;
	const struct pack_idx_entry * const *entry2 = e2;
	if ((*entry1)->offset < (*entry2)->offset)
		return -1;
	if ((*entry1)->offset > (*entry2)->offset)
		return 1;
	return 0;
}

static struct pack_idx_entry **sort_objs_by_offset(struct pack_idx_entry *list,
						    unsigned nr_objects)
{
	unsigned i;
	struct pack_idx_entry **sorted;

	sorted = xmalloc((nr_objects + 1) * sizeof(*sorted));
	for (i = 0; i < nr_objects + 1; i++)
		sorted[i] = &list[i];
	qsort(sorted, nr_objects + 1, sizeof(*sorted), sort_by_offset);

	return sorted;
}

static int create_pack_dictionaries(struct packed_git *p,
				    struct pack_idx_entry **obj_list)
{
	struct progress *progress_state;
	unsigned int i;

	progress_state = start_progress("Scanning objects", p->num_objects);
	for (i = 0; i < p->num_objects; i++) {
		struct pack_idx_entry *obj = obj_list[i];
		void *data;
		enum object_type type;
		unsigned long size;
		struct object_info oi = {};
		int (*add_dict_entries)(void *, unsigned long);

		display_progress(progress_state, i+1);

		oi.typep = &type;
		oi.sizep = &size;
		if (packed_object_info(p, obj->offset, &oi) < 0)
			die("cannot get type of %s from %s",
			    sha1_to_hex(obj->sha1), p->pack_name);

		switch (type) {
		case OBJ_COMMIT:
			add_dict_entries = add_commit_dict_entries;
			break;
		case OBJ_TREE:
			add_dict_entries = add_tree_dict_entries;
			break;
		default:
			continue;
		}
		data = unpack_entry(p, obj->offset, &type, &size);
		if (!data)
			die("cannot unpack %s from %s",
			    sha1_to_hex(obj->sha1), p->pack_name);
		if (check_sha1_signature(obj->sha1, data, size, typename(type)))
			die("packed %s from %s is corrupt",
			    sha1_to_hex(obj->sha1), p->pack_name);
		if (add_dict_entries(data, size) < 0)
			die("can't process %s object %s",
				typename(type), sha1_to_hex(obj->sha1));
		free(data);
	}

	stop_progress(&progress_state);
	return 0;
}

static unsigned long write_dict_table(struct sha1file *f, struct dict_table *t)
{
	unsigned char buffer[1024], *end;
	unsigned hdrlen;
	unsigned long size, datalen;
	z_stream stream;
	int i, status;

	/*
	 * Stored dict table format: uncompressed data length followed by
	 * compressed content.
	 */

	datalen = t->ptr;
	end = add_number(buffer, datalen);
	hdrlen = end - buffer;
	sha1write(f, buffer, hdrlen);

	memset(&stream, 0, sizeof(stream));
	deflateInit(&stream, pack_compression_level);

	for (i = 0; i < t->nb_entries; i++) {
		stream.next_in = t->data + t->entry[i].offset;
		stream.avail_in = 2 + strlen((char *)t->data + t->entry[i].offset + 2) + 1;
		do {
			stream.next_out = (unsigned char *)buffer;
			stream.avail_out = sizeof(buffer);
			status = deflate(&stream, 0);
			size = stream.next_out - (unsigned char *)buffer;
			sha1write(f, buffer, size);
		} while (status == Z_OK);
	}
	do {
		stream.next_out = (unsigned char *)buffer;
		stream.avail_out = sizeof(buffer);
		status = deflate(&stream, Z_FINISH);
		size = stream.next_out - (unsigned char *)buffer;
		sha1write(f, buffer, size);
	} while (status == Z_OK);
	if (status != Z_STREAM_END)
		die("unable to deflate dictionary table (%d)", status);
	if (stream.total_in != datalen)
		die("dict data size mismatch (%ld vs %ld)",
		    stream.total_in, datalen);
	deflateEnd(&stream);

	return hdrlen + datalen;
}

static struct sha1file * packv4_open(char *path)
{
	int fd;

	fd = open(path, O_CREAT|O_EXCL|O_WRONLY, 0600);
	if (fd < 0)
		die_errno("unable to create '%s'", path);
	return sha1fd(fd, path);
}

static unsigned int packv4_write_header(struct sha1file *f, unsigned nr_objects)
{
	struct pack_header hdr;

	hdr.hdr_signature = htonl(PACK_SIGNATURE);
	hdr.hdr_version = htonl(4);
	hdr.hdr_entries = htonl(nr_objects);
	sha1write(f, &hdr, sizeof(hdr));

	return sizeof(hdr);
}

static unsigned long packv4_write_tables(struct sha1file *f, unsigned nr_objects,
					 struct pack_idx_entry *objs)
{
	unsigned i;
	unsigned long written = 0;

	/* The sorted list of object SHA1's is always first */
	for (i = 0; i < nr_objects; i++)
		sha1write(f, objs[i].sha1, 20);
	written = 20 * nr_objects;

	/* Then the commit dictionary table */
	written += write_dict_table(f, commit_name_table);

	/* Followed by the path component dictionary table */
	written += write_dict_table(f, tree_path_table);

	return written;
}

static unsigned int write_object_header(struct sha1file *f, enum object_type type, unsigned long size)
{
	unsigned char buf[30], *end;
	uint64_t val;

	/*
	 * We really have only one kind of delta object.
	 */
	if (type == OBJ_OFS_DELTA)
		type = OBJ_REF_DELTA;

	/*
	 * We allocate 4 bits in the LSB for the object type which should
	 * be good for quite a while, given that we effectively encodes
	 * only 5 object types: commit, tree, blob, delta, tag.
	 */
	val = size;
	if (MSB(val, 4))
		die("fixme: the code doesn't currently cope with big sizes");
	val <<= 4;
	val |= type;
	end = add_number(buf, val);
	sha1write(f, buf, end - buf);
	return end - buf;
}

static unsigned long copy_object_data(struct sha1file *f, struct packed_git *p,
				      off_t offset)
{
	struct pack_window *w_curs = NULL;
	struct revindex_entry *revidx;
	enum object_type type;
	unsigned long avail, size, datalen, written;
	int hdrlen, idx_nr;
	unsigned char *src, *end, buf[24];

	revidx = find_pack_revindex(p, offset);
	idx_nr = revidx->nr;
	datalen = revidx[1].offset - offset;

	src = use_pack(p, &w_curs, offset, &avail);
	hdrlen = unpack_object_header_buffer(src, avail, &type, &size);

	written = write_object_header(f, type, size);

	if (type == OBJ_OFS_DELTA) {
		unsigned char c = src[hdrlen++];
		off_t base_offset = c & 127;
		while (c & 128) {
			base_offset += 1;
			if (!base_offset || MSB(base_offset, 7))
				die("delta offset overflow");
			c = src[hdrlen++];
			base_offset = (base_offset << 7) + (c & 127);
		}
		base_offset = offset - base_offset;
		if (base_offset <= 0 || base_offset >= offset)
			die("delta offset out of bound");
		revidx = find_pack_revindex(p, base_offset);
		end = add_sha1_ref(buf, nth_packed_object_sha1(p, revidx->nr));
		sha1write(f, buf, end - buf);
		written += end - buf;
	} else if (type == OBJ_REF_DELTA) {
		end = add_sha1_ref(buf, src + hdrlen);
		hdrlen += 20;
		sha1write(f, buf, end - buf);
		written += end - buf;
	}

	if (p->index_version > 1 &&
	    check_pack_crc(p, &w_curs, offset, datalen, idx_nr))
		die("bad CRC for object at offset %"PRIuMAX" in %s",
		    (uintmax_t)offset, p->pack_name);

	offset += hdrlen;
	datalen -= hdrlen;

	while (datalen) {
		src = use_pack(p, &w_curs, offset, &avail);
		if (avail > datalen)
			avail = datalen;
		sha1write(f, src, avail);
		written += avail;
		offset += avail;
		datalen -= avail;
	}
	unuse_pack(&w_curs);

	return written;
}

static unsigned char *get_delta_base(struct packed_git *p, off_t offset,
				     unsigned char *sha1_buf)
{
	struct pack_window *w_curs = NULL;
	enum object_type type;
	unsigned long avail, size;
	int hdrlen;
	unsigned char *src;
	const unsigned char *base_sha1 = NULL; ;

	src = use_pack(p, &w_curs, offset, &avail);
	hdrlen = unpack_object_header_buffer(src, avail, &type, &size);

	if (type == OBJ_OFS_DELTA) {
		struct revindex_entry *revidx;
		unsigned char c = src[hdrlen++];
		off_t base_offset = c & 127;
		while (c & 128) {
			base_offset += 1;
			if (!base_offset || MSB(base_offset, 7))
				error("delta offset overflow");
			c = src[hdrlen++];
			base_offset = (base_offset << 7) + (c & 127);
		}
		base_offset = offset - base_offset;
		if (base_offset <= 0 || base_offset >= offset)
			error("delta offset out of bound");
		revidx = find_pack_revindex(p, base_offset);
		base_sha1 = nth_packed_object_sha1(p, revidx->nr);
	} else if (type == OBJ_REF_DELTA) {
		base_sha1 = src + hdrlen;
	} else {
		error("expected to get a delta but got a %s", typename(type));
	}

	unuse_pack(&w_curs);

	if (!base_sha1)
		return NULL;
	hashcpy(sha1_buf, base_sha1);
	return sha1_buf;
}

static off_t packv4_write_object(struct sha1file *f, struct packed_git *p,
				 struct pack_idx_entry *obj)
{
	void *src, *result;
	struct object_info oi = {};
	enum object_type type, packed_type;
	unsigned long size;
	unsigned int hdrlen;

	oi.typep = &type;
	oi.sizep = &size;
	packed_type = packed_object_info(p, obj->offset, &oi);
	if (packed_type < 0)
		die("cannot get type of %s from %s",
		    sha1_to_hex(obj->sha1), p->pack_name);

	/* Some objects are copied without decompression */
	switch (type) {
	case OBJ_COMMIT:
	case OBJ_TREE:
		break;
	default:
		return copy_object_data(f, p, obj->offset);
	}

	/* The rest is converted into their new format */
	src = unpack_entry(p, obj->offset, &type, &size);
	if (!src)
		die("cannot unpack %s from %s",
		    sha1_to_hex(obj->sha1), p->pack_name);
	if (check_sha1_signature(obj->sha1, src, size, typename(type)))
		die("packed %s from %s is corrupt",
		    sha1_to_hex(obj->sha1), p->pack_name);

	switch (type) {
	case OBJ_COMMIT:
		result = conv_to_dict_commit(src, &size);
		break;
	case OBJ_TREE:
		if (packed_type != OBJ_TREE) {
			unsigned char sha1_buf[20], *ref_sha1;
			void *ref;
			enum object_type ref_type;
			unsigned long ref_size;

			ref_sha1 = get_delta_base(p, obj->offset, sha1_buf);
			if (!ref_sha1)
				die("unable to get delta base sha1 for %s",
						sha1_to_hex(obj->sha1));
			ref = read_sha1_file(ref_sha1, &ref_type, &ref_size);
			if (!ref || ref_type != OBJ_TREE)
				die("cannot obtain delta base for %s",
						sha1_to_hex(obj->sha1));
			result = conv_to_dict_tree(src, &size,
					ref, ref_size, ref_sha1);
			free(ref);
		} else {
			result = conv_to_dict_tree(src, &size, NULL, 0, NULL);
		}
		break;
	default:
		die("unexpected object type %d", type);
	}
	free(src);
	if (!result && size)
		die("can't convert %s object %s",
		    typename(type), sha1_to_hex(obj->sha1));
	hdrlen = write_object_header(f, type, size);
	sha1write(f, result, size);
	free(result);
	return hdrlen + size;
}

static char *normalize_pack_name(const char *path)
{
	char buf[PATH_MAX];
	int len;

	len = strlcpy(buf, path, PATH_MAX);
	if (len >= PATH_MAX - 6)
		die("name too long: %s", path);

	/*
	 * In addition to "foo.idx" we accept "foo.pack" and "foo";
	 * normalize these forms to "foo.pack".
	 */
	if (has_extension(buf, ".idx")) {
		strcpy(buf + len - 4, ".pack");
		len++;
	} else if (!has_extension(buf, ".pack")) {
		strcpy(buf + len, ".pack");
		len += 5;
	}

	return xstrdup(buf);
}

static struct packed_git *open_pack(const char *path)
{
	char *packname = normalize_pack_name(path);
	int len = strlen(packname);
	struct packed_git *p;

	strcpy(packname + len - 5, ".idx");
	p = add_packed_git(packname, len - 1, 1);
	if (!p)
		die("packfile %s not found.", packname);

	install_packed_git(p);
	if (open_pack_index(p))
		die("packfile %s index not opened", p->pack_name);

	free(packname);
	return p;
}

static void process_one_pack(char *src_pack, char *dst_pack)
{
	struct packed_git *p;
	struct sha1file *f;
	struct pack_idx_entry *objs, **p_objs;
	struct pack_idx_option idx_opts;
	unsigned i, nr_objects;
	off_t written = 0;
	char *packname;
	unsigned char pack_sha1[20];
	struct progress *progress_state;

	p = open_pack(src_pack);
	if (!p)
		die("unable to open source pack");

	nr_objects = p->num_objects;
	objs = get_packed_object_list(p);
	p_objs = sort_objs_by_offset(objs, nr_objects);

	create_pack_dictionaries(p, p_objs);
	sort_dict_entries_by_hits(commit_name_table);
	sort_dict_entries_by_hits(tree_path_table);

	packname = normalize_pack_name(dst_pack);
	f = packv4_open(packname);
	if (!f)
		die("unable to open destination pack");
	written += packv4_write_header(f, nr_objects);
	written += packv4_write_tables(f, nr_objects, objs);

	/* Let's write objects out, updating the object index list in place */
	progress_state = start_progress("Writing objects", nr_objects);
	all_objs = objs;
	all_objs_nr = nr_objects;
	for (i = 0; i < nr_objects; i++) {
		off_t obj_pos = written;
		struct pack_idx_entry *obj = p_objs[i];
		crc32_begin(f);
		written += packv4_write_object(f, p, obj);
		obj->offset = obj_pos;
		obj->crc32 = crc32_end(f);
		display_progress(progress_state, i+1);
	}
	stop_progress(&progress_state);

	sha1close(f, pack_sha1, CSUM_CLOSE | CSUM_FSYNC);

	reset_pack_idx_option(&idx_opts);
	idx_opts.version = 3;
	strcpy(packname + strlen(packname) - 5, ".idx");
	write_idx_file(packname, p_objs, nr_objects, &idx_opts, pack_sha1);

	free(packname);
}

static int git_pack_config(const char *k, const char *v, void *cb)
{
	if (!strcmp(k, "pack.compression")) {
		int level = git_config_int(k, v);
		if (level == -1)
			level = Z_DEFAULT_COMPRESSION;
		else if (level < 0 || level > Z_BEST_COMPRESSION)
			die("bad pack compression level %d", level);
		pack_compression_level = level;
		pack_compression_seen = 1;
		return 0;
	}
	return git_default_config(k, v, cb);
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <src_packfile> <dst_packfile>\n", argv[0]);
		exit(1);
	}
	git_config(git_pack_config, NULL);
	if (!pack_compression_seen && core_compression_seen)
		pack_compression_level = core_compression_level;
	process_one_pack(argv[1], argv[2]);
	if (0)
		dict_dump();
	return 0;
}
