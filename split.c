/*
 * git-split: split large files based on content-defined breakpoints
 *
 * Copyright (c) Geert Bosch, 2014
 */

#include "cache.h"
#include "strbuf.h"
#include "blob.h"
#include "tree.h"
#include "commit.h"
#include "tag.h"
#include "pack.h"
#include "csum-file.h"

static const unsigned chunk_size = 0x1000; /* Must be power of two, >= 256 */
static const unsigned max_chunk_size = 4 * chunk_size;
static const unsigned max_pack_size = 64 * 1024 * 1024;
static const unsigned max_record_size = 20 * 1024 * 1024;

static int write_object = 1;
static int full = 0;
static int print_tree = 0;
static int verbose = 0;

static enum record_splitting_mode { no_records, bson_records } record_mode;

#define RABIN_POLY 0x9295a59fULL
#define RABIN_DEGREE 31
#define RABIN_SHIFT 23
#define RABIN_WINDOW_SIZE 48

unsigned long long T[256] =
{ 0x00000000, 0x9295a59f, 0x252b4b3e, 0xb7beeea1, 0x4a56967c, 0xd8c333e3,
  0x6f7ddd42, 0xfde878dd, 0x06388967, 0x94ad2cf8, 0x2313c259, 0xb18667c6,
  0x4c6e1f1b, 0xdefbba84, 0x69455425, 0xfbd0f1ba, 0x0c7112ce, 0x9ee4b751,
  0x295a59f0, 0xbbcffc6f, 0x462784b2, 0xd4b2212d, 0x630ccf8c, 0xf1996a13,
  0x0a499ba9, 0x98dc3e36, 0x2f62d097, 0xbdf77508, 0x401f0dd5, 0xd28aa84a,
  0x653446eb, 0xf7a1e374, 0x18e2259c, 0x8a778003, 0x3dc96ea2, 0xaf5ccb3d,
  0x52b4b3e0, 0xc021167f, 0x779ff8de, 0xe50a5d41, 0x1edaacfb, 0x8c4f0964,
  0x3bf1e7c5, 0xa964425a, 0x548c3a87, 0xc6199f18, 0x71a771b9, 0xe332d426,
  0x14933752, 0x860692cd, 0x31b87c6c, 0xa32dd9f3, 0x5ec5a12e, 0xcc5004b1,
  0x7beeea10, 0xe97b4f8f, 0x12abbe35, 0x803e1baa, 0x3780f50b, 0xa5155094,
  0x58fd2849, 0xca688dd6, 0x7dd66377, 0xef43c6e8, 0x31c44b38, 0xa351eea7,
  0x14ef0006, 0x867aa599, 0x7b92dd44, 0xe90778db, 0x5eb9967a, 0xcc2c33e5,
  0x37fcc25f, 0xa56967c0, 0x12d78961, 0x80422cfe, 0x7daa5423, 0xef3ff1bc,
  0x58811f1d, 0xca14ba82, 0x3db559f6, 0xaf20fc69, 0x189e12c8, 0x8a0bb757,
  0x77e3cf8a, 0xe5766a15, 0x52c884b4, 0xc05d212b, 0x3b8dd091, 0xa918750e,
  0x1ea69baf, 0x8c333e30, 0x71db46ed, 0xe34ee372, 0x54f00dd3, 0xc665a84c,
  0x29266ea4, 0xbbb3cb3b, 0x0c0d259a, 0x9e988005, 0x6370f8d8, 0xf1e55d47,
  0x465bb3e6, 0xd4ce1679, 0x2f1ee7c3, 0xbd8b425c, 0x0a35acfd, 0x98a00962,
  0x654871bf, 0xf7ddd420, 0x40633a81, 0xd2f69f1e, 0x25577c6a, 0xb7c2d9f5,
  0x007c3754, 0x92e992cb, 0x6f01ea16, 0xfd944f89, 0x4a2aa128, 0xd8bf04b7,
  0x236ff50d, 0xb1fa5092, 0x0644be33, 0x94d11bac, 0x69396371, 0xfbacc6ee,
  0x4c12284f, 0xde878dd0, 0x63889670, 0xf11d33ef, 0x46a3dd4e, 0xd43678d1,
  0x29de000c, 0xbb4ba593, 0x0cf54b32, 0x9e60eead, 0x65b01f17, 0xf725ba88,
  0x409b5429, 0xd20ef1b6, 0x2fe6896b, 0xbd732cf4, 0x0acdc255, 0x985867ca,
  0x6ff984be, 0xfd6c2121, 0x4ad2cf80, 0xd8476a1f, 0x25af12c2, 0xb73ab75d,
  0x008459fc, 0x9211fc63, 0x69c10dd9, 0xfb54a846, 0x4cea46e7, 0xde7fe378,
  0x23979ba5, 0xb1023e3a, 0x06bcd09b, 0x94297504, 0x7b6ab3ec, 0xe9ff1673,
  0x5e41f8d2, 0xccd45d4d, 0x313c2590, 0xa3a9800f, 0x14176eae, 0x8682cb31,
  0x7d523a8b, 0xefc79f14, 0x587971b5, 0xcaecd42a, 0x3704acf7, 0xa5910968,
  0x122fe7c9, 0x80ba4256, 0x771ba122, 0xe58e04bd, 0x5230ea1c, 0xc0a54f83,
  0x3d4d375e, 0xafd892c1, 0x18667c60, 0x8af3d9ff, 0x71232845, 0xe3b68dda,
  0x5408637b, 0xc69dc6e4, 0x3b75be39, 0xa9e01ba6, 0x1e5ef507, 0x8ccb5098,
  0x524cdd48, 0xc0d978d7, 0x77679676, 0xe5f233e9, 0x181a4b34, 0x8a8feeab,
  0x3d31000a, 0xafa4a595, 0x5474542f, 0xc6e1f1b0, 0x715f1f11, 0xe3caba8e,
  0x1e22c253, 0x8cb767cc, 0x3b09896d, 0xa99c2cf2, 0x5e3dcf86, 0xcca86a19,
  0x7b1684b8, 0xe9832127, 0x146b59fa, 0x86fefc65, 0x314012c4, 0xa3d5b75b,
  0x580546e1, 0xca90e37e, 0x7d2e0ddf, 0xefbba840, 0x1253d09d, 0x80c67502,
  0x37789ba3, 0xa5ed3e3c, 0x4aaef8d4, 0xd83b5d4b, 0x6f85b3ea, 0xfd101675,
  0x00f86ea8, 0x926dcb37, 0x25d32596, 0xb7468009, 0x4c9671b3, 0xde03d42c,
  0x69bd3a8d, 0xfb289f12, 0x06c0e7cf, 0x94554250, 0x23ebacf1, 0xb17e096e,
  0x46dfea1a, 0xd44a4f85, 0x63f4a124, 0xf16104bb, 0x0c897c66, 0x9e1cd9f9,
  0x29a23758, 0xbb3792c7, 0x40e7637d, 0xd272c6e2, 0x65cc2843, 0xf7598ddc,
  0x0ab1f501, 0x9824509e, 0x2f9abe3f, 0xbd0f1ba0
};

unsigned long long U[256] =
{ 0x00000000, 0x62f53c8a, 0x577fdc8b, 0x358ae001, 0x3c6a1c89, 0x5e9f2003,
  0x6b15c002, 0x09e0fc88, 0x78d43912, 0x1a210598, 0x2fabe599, 0x4d5ed913,
  0x44be259b, 0x264b1911, 0x13c1f910, 0x7134c59a, 0x633dd7bb, 0x01c8eb31,
  0x34420b30, 0x56b737ba, 0x5f57cb32, 0x3da2f7b8, 0x082817b9, 0x6add2b33,
  0x1be9eea9, 0x791cd223, 0x4c963222, 0x2e630ea8, 0x2783f220, 0x4576ceaa,
  0x70fc2eab, 0x12091221, 0x54ee0ae9, 0x361b3663, 0x0391d662, 0x6164eae8,
  0x68841660, 0x0a712aea, 0x3ffbcaeb, 0x5d0ef661, 0x2c3a33fb, 0x4ecf0f71,
  0x7b45ef70, 0x19b0d3fa, 0x10502f72, 0x72a513f8, 0x472ff3f9, 0x25dacf73,
  0x37d3dd52, 0x5526e1d8, 0x60ac01d9, 0x02593d53, 0x0bb9c1db, 0x694cfd51,
  0x5cc61d50, 0x3e3321da, 0x4f07e440, 0x2df2d8ca, 0x187838cb, 0x7a8d0441,
  0x736df8c9, 0x1198c443, 0x24122442, 0x46e718c8, 0x3b49b04d, 0x59bc8cc7,
  0x6c366cc6, 0x0ec3504c, 0x0723acc4, 0x65d6904e, 0x505c704f, 0x32a94cc5,
  0x439d895f, 0x2168b5d5, 0x14e255d4, 0x7617695e, 0x7ff795d6, 0x1d02a95c,
  0x2888495d, 0x4a7d75d7, 0x587467f6, 0x3a815b7c, 0x0f0bbb7d, 0x6dfe87f7,
  0x641e7b7f, 0x06eb47f5, 0x3361a7f4, 0x51949b7e, 0x20a05ee4, 0x4255626e,
  0x77df826f, 0x152abee5, 0x1cca426d, 0x7e3f7ee7, 0x4bb59ee6, 0x2940a26c,
  0x6fa7baa4, 0x0d52862e, 0x38d8662f, 0x5a2d5aa5, 0x53cda62d, 0x31389aa7,
  0x04b27aa6, 0x6647462c, 0x177383b6, 0x7586bf3c, 0x400c5f3d, 0x22f963b7,
  0x2b199f3f, 0x49eca3b5, 0x7c6643b4, 0x1e937f3e, 0x0c9a6d1f, 0x6e6f5195,
  0x5be5b194, 0x39108d1e, 0x30f07196, 0x52054d1c, 0x678fad1d, 0x057a9197,
  0x744e540d, 0x16bb6887, 0x23318886, 0x41c4b40c, 0x48244884, 0x2ad1740e,
  0x1f5b940f, 0x7daea885, 0x7693609a, 0x14665c10, 0x21ecbc11, 0x4319809b,
  0x4af97c13, 0x280c4099, 0x1d86a098, 0x7f739c12, 0x0e475988, 0x6cb26502,
  0x59388503, 0x3bcdb989, 0x322d4501, 0x50d8798b, 0x6552998a, 0x07a7a500,
  0x15aeb721, 0x775b8bab, 0x42d16baa, 0x20245720, 0x29c4aba8, 0x4b319722,
  0x7ebb7723, 0x1c4e4ba9, 0x6d7a8e33, 0x0f8fb2b9, 0x3a0552b8, 0x58f06e32,
  0x511092ba, 0x33e5ae30, 0x066f4e31, 0x649a72bb, 0x227d6a73, 0x408856f9,
  0x7502b6f8, 0x17f78a72, 0x1e1776fa, 0x7ce24a70, 0x4968aa71, 0x2b9d96fb,
  0x5aa95361, 0x385c6feb, 0x0dd68fea, 0x6f23b360, 0x66c34fe8, 0x04367362,
  0x31bc9363, 0x5349afe9, 0x4140bdc8, 0x23b58142, 0x163f6143, 0x74ca5dc9,
  0x7d2aa141, 0x1fdf9dcb, 0x2a557dca, 0x48a04140, 0x399484da, 0x5b61b850,
  0x6eeb5851, 0x0c1e64db, 0x05fe9853, 0x670ba4d9, 0x528144d8, 0x30747852,
  0x4ddad0d7, 0x2f2fec5d, 0x1aa50c5c, 0x785030d6, 0x71b0cc5e, 0x1345f0d4,
  0x26cf10d5, 0x443a2c5f, 0x350ee9c5, 0x57fbd54f, 0x6271354e, 0x008409c4,
  0x0964f54c, 0x6b91c9c6, 0x5e1b29c7, 0x3cee154d, 0x2ee7076c, 0x4c123be6,
  0x7998dbe7, 0x1b6de76d, 0x128d1be5, 0x7078276f, 0x45f2c76e, 0x2707fbe4,
  0x56333e7e, 0x34c602f4, 0x014ce2f5, 0x63b9de7f, 0x6a5922f7, 0x08ac1e7d,
  0x3d26fe7c, 0x5fd3c2f6, 0x1934da3e, 0x7bc1e6b4, 0x4e4b06b5, 0x2cbe3a3f,
  0x255ec6b7, 0x47abfa3d, 0x72211a3c, 0x10d426b6, 0x61e0e32c, 0x0315dfa6,
  0x369f3fa7, 0x546a032d, 0x5d8affa5, 0x3f7fc32f, 0x0af5232e, 0x68001fa4,
  0x7a090d85, 0x18fc310f, 0x2d76d10e, 0x4f83ed84, 0x4663110c, 0x24962d86,
  0x111ccd87, 0x73e9f10d, 0x02dd3497, 0x6028081d, 0x55a2e81c, 0x3757d496,
  0x3eb7281e, 0x5c421494, 0x69c8f495, 0x0b3dc81f
};

const unsigned blob_mode = 0100644;
const unsigned tree_mode = 0040000;
               
typedef struct _tree_ent {
        struct object *obj;
        unsigned mode;
        uint64_t offset;
} tree_ent;
tree_ent *splits;
static int alloc_splits, used_splits;

/* The fast_pack struct keeps all information necessary for incrementally
   updating the contents of a pack. */

typedef struct _fast_pack {
        z_stream *stream;
        unsigned char *data;
        unsigned size;
        unsigned alloc;
        unsigned objects;
} fast_pack;

typedef struct _obj_stats {
        unsigned new_objs;
        unsigned total_objs;
        uint64_t new_bytes;
        uint64_t total_bytes;
        uint64_t pack_bytes;
} obj_stats;

static obj_stats blob_stats, tree_stats;

static void append_to_tree(unsigned mode, unsigned char *sha1, uint64_t offset);
static void split_tree(fast_pack *pack, unsigned char *sha1,
                       tree_ent *tree, int count);
static void finish_pack (fast_pack *pack, unsigned char *returnsha1);
static void append_to_pack(void *in, unsigned size, const char *type,
                           fast_pack *pack, unsigned char *returnsha1);

static void append_to_tree(unsigned mode, unsigned char *sha1, uint64_t offset)
{
        static uint64_t last_offset;
        tree_ent *ent;

        if (last_offset && offset == last_offset)
                die("duplicate tree entry %s at %llu\n", sha1_to_hex (sha1),
                    (long long unsigned) offset);
        last_offset = offset;

        if (alloc_splits <= used_splits) {
                alloc_splits = alloc_nr(used_splits);
                splits = xrealloc(splits, sizeof(tree_ent)
                        * alloc_splits);
        }
        ent = splits + used_splits++;
        ent->mode = mode;
        ent->offset = offset;
        ent->obj = xmalloc(sizeof(struct object));
        memset(ent->obj, 0, sizeof(struct object));
        create_object(sha1, OBJ_NONE, ent->obj);
}

static unsigned write_tree (fast_pack *pack, unsigned char *sha1,
                            tree_ent *tree, int count)
{
        const int MAX_TREE_ENT_SIZE = 100;
        char buffer[max_chunk_size + MAX_TREE_ENT_SIZE];
        char mode[32];

        uint64_t ofs_max = tree[count - 1].offset - tree->offset;
        unsigned  ofs_width = sprintf(mode, "%llu",
                                      (long long unsigned) ofs_max);
        unsigned  num_width = sprintf(mode, "%i", count);
        tree_ent *ent;
        unsigned out = 0;

        for (ent = tree; ent < tree + count; ent++)
        {
		int start = out;

                if (record_mode) {
			out += sprintf(buffer + out, "%o %0*li", ent->mode,
                                       num_width, ent - tree);
		} else {
			long long unsigned ofs = ent->offset - tree->offset;
			out += sprintf(buffer + out, "%o %0*llu", ent->mode,
                                       ofs_width, ofs);
                }

		buffer[out++] = 0;

                if (print_tree) {
                         printf("%s\t%s\n", buffer + start,
                                sha1_to_hex(ent->obj->sha1));
                }

                hashcpy((unsigned char*)buffer + out, ent->obj->sha1);
                out += 20;
        }

        append_to_pack(buffer, out, tree_type, pack, sha1);
        return out;
}

/* Returns number of entries before break, or count if no break occurs */
static int find_tree_break (tree_ent *tree, int count)
{
        /* we only break at tree entry boundaries, and each entry is
           about 32 bytes, so consider 5 less bits for the test  */

        const unsigned mask = chunk_size / 32 - 1;

        char mode[32];

        /* Each tree entry has a mode, space, path, 20-byte SHA1, and nul.
           The path is the decimal offset relative to the tree start, right
           aligned with zeros. As more digits are needed to represent this
           offset, ent_size increases.  */
        unsigned ent_size = 21 + sprintf (mode, "%o 0", tree->mode);
        long long unsigned max_ofs = 9;

        int j;

        for (j = 1; j < count; j++) {
                while (tree[j].offset - tree->offset > max_ofs) {
                        ent_size++;
                        max_ofs = 10*max_ofs + 9;
                }
                if ((j + 1) * ent_size > max_chunk_size) return j;
                if (!(tree[j].obj->sha1[19] & mask)) return j + 1;
        }

        return j;
}

static void split_tree(fast_pack *pack, unsigned char *sha1,
                       tree_ent *tree, int count)
{
        int first_split = used_splits;
        uint64_t first_offset = tree->offset;

        while (count) {
                int brk = find_tree_break(tree, count);
                uint64_t offset = tree->offset - first_offset;
                write_tree(pack, sha1, tree, brk);
                count -= brk;
                tree += brk;

                /* If entries are left, or splits have been made,
                   create a new tree entry for the next higher level,
                   using an offset relative to that tree's start. */

                if (count)
                        append_to_tree(tree_mode, sha1, offset);
                else if (used_splits > first_split) {
                        append_to_tree(tree_mode, sha1, offset);
                        count = used_splits - first_split;
                        first_split = used_splits;
                        first_offset = tree->offset;
                }
        }
}

static void finish_pack (fast_pack *pack, unsigned char *returnsha1)
{
        static int pack_nr = 0;
        unsigned char pack_file_sha1[20];
        struct sha1file *f;
        struct pack_header hdr;
        const char *pack_tmp_name;
        char tmpname[PATH_MAX];

        if (!pack || !pack->objects)
                return;

        /* FIXME: dump pack to pipe to git-index-pack --stdin --keep */
        {
                int fd;
                /* While still using temporary files for packs, be helpful
                   and number them, so they sort in the order of creation. */
                snprintf(tmpname, sizeof(tmpname), "split_tmp%04iXXXXXX",
                         pack_nr++);
                fd = mkstemp(tmpname);
                if (fd < 0)
                        die("unable to create %s: %s\n", tmpname,
                            strerror(errno));
                pack_tmp_name = xstrdup(tmpname);
                f = sha1fd(fd, pack_tmp_name);
        }

        hdr.hdr_signature = htonl(PACK_SIGNATURE);
        hdr.hdr_version = htonl(PACK_VERSION);
        hdr.hdr_entries = htonl(pack->objects);
        sha1write(f, &hdr, sizeof(hdr));
        sha1write(f, pack->data, pack->size);
        sha1close(f, pack_file_sha1, 1);

        pack->objects = 0;
        pack->size = 0;

        if (returnsha1)
                hashcpy(returnsha1, pack_file_sha1);
}

static void append_to_pack(void *in, unsigned size, const char *type,
                           fast_pack *pack, unsigned char *returnsha1)
{
        z_stream stream;
        const int MAX_PACK_HDR_LEN = 10;
        const int PACK_HEADER_FOOTER = sizeof(struct pack_header) + 20;
        unsigned char sha1[20];

        obj_stats *stats = (type == blob_type) ? &blob_stats : &tree_stats;

        stats->total_objs++;
        stats->total_bytes += size;

        hash_sha1_file(in, size, type, sha1);

        if (returnsha1)
                hashcpy(returnsha1, sha1);

        if(!pack || lookup_object(sha1))
                return;

        if (!full && has_sha1_file(sha1))
                return;

        stats->new_objs++;
        stats->new_bytes += size;

        /* Need to init stream for deflateBound */
        memset(&stream, 0, sizeof(stream));
        deflateInit(&stream, zlib_compression_level);

        /* Have to start new pack before adding this object  */
        if (pack->size + MAX_PACK_HDR_LEN + deflateBound(&stream, size)
                        + PACK_HEADER_FOOTER > max_pack_size) {
                finish_pack(pack, 0);
        }

        if (pack->alloc <= pack->size + MAX_PACK_HDR_LEN) {
                pack->alloc = alloc_nr(pack->alloc + MAX_PACK_HDR_LEN);
                pack->data = xrealloc(pack->data, pack->alloc);
        }

        pack->size += encode_in_pack_object_header(type_from_string(type), size,
                                                   pack->data + pack->size);

        /* Then the data itself... */
        stream.next_in = in;
        stream.avail_in = size;

        /* Compress to ... */
        stream.next_out = pack->data + pack->size;
        stream.avail_out =pack->alloc - pack->size;

        do {
                unsigned off = pack->alloc - stream.avail_out;

                if (pack->alloc == off) {
                        pack->alloc = alloc_nr(pack->alloc);
                        pack->data = xrealloc(pack->data, pack->alloc);
                }

                stream.next_out = pack->data + off;
                stream.avail_out = pack->alloc - off;
        } while (deflate(&stream, Z_FINISH) == Z_OK);

        deflateEnd(&stream);

        pack->objects++;
        pack->size += stream.total_out;

        stats->pack_bytes += stream.total_out;
}

static int block_read_pipe(int fd, char *buf, unsigned long *size)
{
        int iret;
        unsigned long off = 0;

        do {
                iret = xread(fd, buf + off, *size - off);
                off += iret;
        } while (iret > 0 && off < *size);

        if (iret < 0)
                return -1;

        *size = off;
        return 0;
}

/* Split the buffer buf[0 .. *size - 1] into objects of blob_type.
   Add objects to tree and optionally write them to disk.
   Returns the number of chars that were processed from buf. */

static unsigned split_buffer(char *buf, unsigned size, uint64_t *offset, 
                             fast_pack *pack)
{
        unsigned char sha1[20];
        unsigned fp = 0;
        unsigned len = 0;
        unsigned remaining = size;
        unsigned breakmask = chunk_size - 1;

        while (len < remaining) {
                int max_len = remaining < max_chunk_size
                              ? remaining : max_chunk_size;

                if (record_mode && max_len <= chunk_size * 2) len = max_len;

                /* Do not allow breaks before window is filled */
                while (len < RABIN_WINDOW_SIZE && len < max_len) {
                        fp = ((fp << 8) | buf[len++]) ^ T[fp >> RABIN_SHIFT];
                }

                /* Find a content-defined breakpoint */
                while (len < max_len && ((fp & breakmask) != (breakmask)))
                {
                        fp ^= U[(int) buf[len - RABIN_WINDOW_SIZE]];
                        fp = ((fp << 8) | buf[len++]) ^ T[fp >> RABIN_SHIFT];
                }

                /* Write to pack if the object is complete, if the entire
                   buffer was an incomplete object, or if we are in record
                   mode, in which case we want to write the incomplete object
                   out to force a new object at the record boundary. */
                if (len < remaining || len == max_chunk_size
                    || remaining == size || record_mode)
                {
                        append_to_pack(buf, len, blob_type, pack, sha1);

                        append_to_tree(blob_mode, sha1, *offset);

                        *offset += len;
                        remaining -= len;
                        buf += len;
                        len = 0;
                        fp = 0;
                }
        }

        return size - remaining;
}

static int split_pipe(int fd, fast_pack *pack)
{
        unsigned long size = max_chunk_size * 16 > max_record_size ?
                             max_chunk_size * 16 : max_record_size * 2;
        unsigned long len = 0;
        uint64_t offset = 0;
	size = 0x100000000;
        char *buf = xmalloc(size);

        do {
                /* Invariant: buf[0 .. len - 1] has partial object or rec */

                unsigned long read_len = size - len;

                /* Read as much into the buffer as will fit, or to EOF */
                if (block_read_pipe(fd, buf + len, &read_len)) {
                        free(buf);
                        return -1;
                }

                read_len += len;

                /* buf[0 .. read_len] is empty, or has a complete object.
                   For record splitting modes, buf has a complete record. */
                if (!read_len)
                        break;

                if (record_mode) {
			unsigned reclen;
                        if (read_len < 6) {
                                die("truncated BSON document at offset 0x%llx",
                                    (long long unsigned) offset);
                        }
                        
			do {
                        	reclen = (unsigned char) buf[len];
                        	reclen += ((unsigned char) buf[len + 1]) << 8;
                        	reclen += ((unsigned char) buf[len + 2]) << 16;
                        	reclen += ((unsigned char) buf[len + 3]) << 24;

				/* Check if a complete record is available. */
				if (len + reclen > read_len) break;

				len += split_buffer (buf + len, reclen, 
                                                     &offset,pack);
			} while (len + 6 < read_len);

                        /* Record is too large to handle */
                        if (reclen > max_record_size) {
                                die("overlong or corrupt BSON document at off"
                                    "set 0x%llx", (long long unsigned) offset);
			}
                } else {
                	/* Split buffer and add all complete objects to pack */
	                len = split_buffer (buf, read_len, &offset, pack);
		}

                /* Move partial object or record to beginning of buffer */
                if (len < read_len) {
                        memmove(buf, buf + len, read_len - len);
                }
                len = read_len - len;
        } while (1);

        free(buf);
        return 0;
}

/* Return pointer to static buffer with formatted division or blank */
static char *format_div (const char *fmt, double num, double den) {
        static char result[16];
        int zerop = (den == 0.0);
        double div = zerop ? den : num / den;
        int len = sprintf(result, fmt, div);

        if (zerop) sprintf(result, "%*s", len, "");
        return result;
}

static char *format_pct (double num, double den) {
        return format_div("(%5.1f%%)", 100.0 * num, den);
}

static void print_stats (obj_stats *stats) {
        unsigned long long total_bytes = stats->total_bytes;
        unsigned long long new_bytes = stats->new_bytes;
        unsigned long long pack_bytes = stats->pack_bytes;
        unsigned total_objs = stats->total_objs;
        unsigned new_objs = stats->new_objs;

        fprintf(stderr, "  total bytes: %10llu  new bytes: %10llu %s",
                total_bytes, new_bytes,
                format_pct((double) new_bytes, (double) total_bytes));

        fprintf(stderr, "  pack bytes: %10llu %s\n", pack_bytes,
                format_pct((double) pack_bytes, (double) total_bytes));

        fprintf(stderr, "   total objs: %10u   new objs: %10u %s\n",
                total_objs, new_objs,
                format_pct((double) new_objs, (double) total_objs));

        fprintf(stderr, "    bytes/obj: %s",
                format_div("%10.0f", (double) total_bytes, (double) total_objs));
        fprintf(stderr, "  bytes/obj: %s\n",
                format_div("%10.0f", (double) new_bytes, (double) new_objs));
}

static const char split_usage[] = "git-split [-f] [-b] [-n] [-p] [-v]";

int main(int argc, char **argv)
{
        int fd = 0;
        unsigned char sha1[20];
        fast_pack pack;

        memset(&pack, 0, sizeof(pack));

        setup_git_directory();

        while ((argc > 1) && argv[1][0] == '-') {
                char *arg = argv[1];
                if (!strcmp("-f", arg))
                        full = 1;
                else if (!strcmp("-b", arg))
                        record_mode = bson_records; 
                else if (!strcmp("-n", arg))
                        write_object = 0;
                else if (!strcmp("-p", arg))
                        print_tree = 1;
                else if (!strcmp("-v", arg))
                        verbose = 1;
                else
                        usage(split_usage);
                argc--;
                argv++;
        }

        do {
                if (argc > 1 && (fd = open(argv[1], O_RDONLY)) < 0)
                        die ("cannot open %s: %s", argv[1], strerror(errno));

                if (split_pipe(fd, write_object ? &pack : 0))
                        die("error reading from pipe");
                if (fd) {
                        close(fd);
                        argc--;
                        argv++;
                }
                split_tree(&pack, sha1, splits, used_splits);
                puts(sha1_to_hex(sha1));
                used_splits = 0;
        } while (fd && argc > 1);
        finish_pack(&pack, 0);

        if (verbose) {
                long long unsigned total_in = blob_stats.total_bytes;
                long long unsigned total_out = blob_stats.pack_bytes +
                                               tree_stats.pack_bytes;
                fprintf(stderr, "blob stats:\n");
                print_stats (&blob_stats);
                fprintf(stderr, "\ntree stats:\n");
                print_stats (&tree_stats);
                fprintf(stderr, "\ntotal in: %llu, total out: %llu %s\n",
                        total_in, total_out,
                        format_pct((double) total_out, (double) total_in));
        }

        exit(0);
}
