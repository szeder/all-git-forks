#include "cache.h"
#include "pack.h"
#include "pack-revindex.h"
#include "progress.h"
#include "varint.h"
#include "packv4-create.h"
#include "tree-walk.h"
#include "packv4-parse.h"

static int pack_compression_seen;
static int pack_compression_level = Z_DEFAULT_COMPRESSION;
extern int min_tree_copy;

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

static struct sha1file * packv4_open(char *path)
{
	int fd;

	fd = open(path, O_CREAT|O_EXCL|O_WRONLY, 0600);
	if (fd < 0)
		die_errno("unable to create '%s'", path);
	return sha1fd(fd, path);
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
	if (ends_with(buf, ".idx")) {
		strcpy(buf + len - 4, ".pack");
		len++;
	} else if (!ends_with(buf, ".pack")) {
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
	if (!is_pack_valid(p) || p->version != 4)
		die("input pack is not v4");

	free(packname);
	return p;
}

static void strbuf_sha1ref(struct strbuf *sb,
			   struct packed_git *p,
			   const unsigned char *sha1)
{
	const unsigned char *sha1_table = p->sha1_table;
	unsigned char buf[16];
	int buflen;

	if (sha1 >= sha1_table && (sha1 - sha1_table) / 20 < p->num_objects) {
		if ((sha1 - sha1_table) % 20)
			die("bad sha1 pointer");
		buflen = encode_varint((sha1 - sha1_table) / 20 + 1, buf);
		strbuf_add(sb, buf, buflen);
	} else {
		strbuf_addch(sb, 0);
		strbuf_add(sb, sha1, 20);
	}
}

static void add_base_sha1_index(struct strbuf *sb,
				struct packed_git *p,
				off_t offset)
{
	unsigned char buf[16];
	int buflen;
	struct revindex_entry *revidx;

	revidx = find_pack_revindex(p, offset);
	buflen = encode_varint(revidx->nr + 1, buf);
	strbuf_add(sb, buf, buflen);
}

static unsigned long copy_object_data(struct sha1file *f, struct packed_git *p,
				      off_t offset)
{
	struct pack_window *w_curs = NULL;
	struct revindex_entry *revidx;
	unsigned long avail, datalen, written;
	unsigned char *src;

	revidx = find_pack_revindex(p, offset);
	datalen = revidx[1].offset - offset;
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

struct tree_entry {
	off_t obj_offset;
	unsigned int path_index;
	unsigned int pos;
	const unsigned char *sha1;
};

static unsigned int nr_jumps;
static unsigned int nr_bases;
static unsigned int nr_copies;
static unsigned int copy_count;
static unsigned int nr_trees;

static void rewrite_tree(struct strbuf *sb,
			 struct packed_git *p,
			 off_t offset)
{
	struct pack_window *w_curs = NULL;
	struct decode_state ds, *dsp;
	unsigned char buf[16];
	int buflen;
	off_t last_copy_base;
	struct tree_entry *entries = NULL;
	int entry_nr = 0, entry_alloc = 0;
	int i, ret, old_nr = nr_jumps;

	/* collect tree entry locations */
	memset(&ds, 0, sizeof(ds));
	ds.p = p;
	ds.w_curs = &w_curs;
	ds.obj_offset = offset;
	dsp = &ds;
	while ((ret = decode_tree_entry(&dsp)) != decode_done) {
		if (ret == decode_failed)
			die("failed to decode");
		if (ret == fallingback)
			die("not support referencing to tree v2");

		ALLOC_GROW(entries, entry_nr + 1, entry_alloc);
		entries[entry_nr].obj_offset = dsp->obj_offset;
		entries[entry_nr].path_index = dsp->path_index;
		entries[entry_nr].sha1	     = dsp->sha1;
		entries[entry_nr].pos	     = dsp->curpos - 1;
		entry_nr++;
		if (dsp != &ds) {
			struct decode_state *p = dsp;
			while (p != &ds) {
				nr_jumps++;
				p = p->up;
			}
		}
	}
	if (nr_jumps == old_nr)
		nr_bases++;

	/* generate new OBJ_PV4_TREE, avoid deep copy sequences */
	buflen = encode_varint(entry_nr, buf);
	strbuf_add(sb, buf, buflen);
	i = 0;
	last_copy_base = 0;
	while (i < entry_nr) {
		struct tree_entry *e = entries + i;
		int j = i + 1;

		while (j < entry_nr &&
		       e->obj_offset == entries[j].obj_offset &&
		       entries[j - 1].pos + 1 == entries[j].pos)
			j++;

		if (j - i > 2 && e->obj_offset != offset) {
			buflen = encode_varint((e->pos << 1) | 1, buf);
			strbuf_add(sb, buf, buflen);
			if (!last_copy_base || last_copy_base != e->obj_offset) {
				buflen = encode_varint(((j - i) << 1) | 1, buf);
				strbuf_add(sb, buf, buflen);
				add_base_sha1_index(sb, p, e->obj_offset);
				last_copy_base = e->obj_offset;
			} else {
				buflen = encode_varint((j - i) << 1, buf);
				strbuf_add(sb, buf, buflen);
			}
			copy_count += j - i;
			nr_copies++;
			i = j;
			continue;
		}

		/*
		 * If we have to encode SHA-1 explicitly, then a copy
		 * sequence of 1 would be shorter. But copy sequences
		 * pay base tree scanning cost until tree offset cache
		 * kicks in...
		 */
		buflen = encode_varint(e->path_index << 1, buf);
		strbuf_add(sb, buf, buflen);
		strbuf_sha1ref(sb, p, e->sha1);
		i++;
	}
	free(entries);
	unuse_pack(&w_curs);
}

static unsigned long convert_v4_tree(struct sha1file *f,
				     struct packed_git *p,
				     off_t offset,
				     unsigned long size)
{
	unsigned long written;
	struct strbuf sb = STRBUF_INIT;
	unsigned char buf[16];
	uint64_t val;
	int len;

	nr_trees++;
	val = size;
	if (MSB(val, 4))
		die("fixme: the code doesn't currently cope with big sizes");
	val <<= 4;
	val |= OBJ_PV4_TREE;
	len = encode_varint(val, buf);
	sha1write(f, buf, len);
	rewrite_tree(&sb, p, offset);
	sha1write(f, sb.buf, sb.len);
	written = sb.len + len;
	strbuf_release(&sb);
	return written;
}

static void process_one_pack(char *src_pack, char *dst_pack)
{
	struct packed_git *p;
	struct sha1file *f;
	struct pack_idx_entry *objs, **p_objs;
	struct pack_idx_option idx_opts;
	unsigned i, nr_objects;
	off_t written, sz;
	char *packname;
	unsigned char pack_sha1[20];
	struct progress *progress_state;
	struct pack_window *w_curs = NULL;
	unsigned long avail;
	const unsigned char *buf;

	p = open_pack(src_pack);
	if (!p)
		die("unable to open source pack");

	nr_objects = p->num_objects;
	objs = get_packed_object_list(p);
	p_objs = sort_objs_by_offset(objs, nr_objects);

	packname = normalize_pack_name(dst_pack);
	f = packv4_open(packname);
	if (!f)
		die("unable to open destination pack");

	/* header and tables */
	sz = p_objs[0]->offset;
	written = 0;
	while (written < sz) {
		buf = use_pack(p, &w_curs, written, &avail);
		if (written + avail > sz)
			avail = sz - written;
		sha1write(f, buf, avail);
		written += avail;
	}
	unuse_pack(&w_curs);

	/* Let's write objects out, updating the object index list in place */
	progress_state = start_progress("Writing objects", nr_objects);
	for (i = 0; i < nr_objects; i++) {
		struct object_info oi = {};
		enum object_type type, packed_type;
		unsigned long obj_size;
		off_t obj_pos = written;
		struct pack_idx_entry *obj = p_objs[i];

		oi.typep = &type;
		oi.sizep = &obj_size;
		packed_type = packed_object_info(p, obj->offset, &oi);
		if (packed_type < 0)
			die("cannot get type of %s from %s",
			    sha1_to_hex(obj->sha1), p->pack_name);

		crc32_begin(f);
		if (packed_type == OBJ_PV4_TREE)
			written += convert_v4_tree(f, p, obj->offset, obj_size);
		else
			written += copy_object_data(f, p, obj->offset);
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
	char *src_pack, *dst_pack;

	if (argc == 3) {
		src_pack = argv[1];
		dst_pack = argv[2];
	} else if (argc == 4 && starts_with(argv[1], "--min-tree-copy=")) {
		min_tree_copy = atoi(argv[1] + strlen("--min-tree-copy="));
		src_pack = argv[2];
		dst_pack = argv[3];
	} else {
		fprintf(stderr, "Usage: %s [--min-tree-copy=<n>] <src_packfile> <dst_packfile>\n", argv[0]);
		exit(1);
	}

	git_config(git_pack_config, NULL);
	if (!pack_compression_seen && core_compression_seen)
		pack_compression_level = core_compression_level;
	process_one_pack(src_pack, dst_pack);
	printf("encountered %u tree jumps\n"
	       "produced a pack with %u copy sequences (copy count %u)\n"
	       "%u tree bases out of %u\n",
	       nr_jumps, nr_copies, copy_count, nr_bases, nr_trees);
	return 0;
}
