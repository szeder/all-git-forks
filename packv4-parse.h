#ifndef PACKV4_PARSE_H
#define PACKV4_PARSE_H

struct packv4_dict {
	const unsigned char *data;
	unsigned int nb_entries;
	unsigned int offsets[FLEX_ARRAY];
};

struct packv4_dict *pv4_create_dict(const unsigned char *data, int dict_size);
void pv4_free_dict(struct packv4_dict *dict);

unsigned long pv4_unpack_object_header_buffer(const unsigned char *base,
					      unsigned long len,
					      enum object_type *type,
					      unsigned long *sizep);
const unsigned char *get_sha1ref(struct packed_git *p,
				 const unsigned char **bufp);

void *pv4_get_commit(struct packed_git *p, struct pack_window **w_curs,
		     off_t offset, unsigned long size);
void *pv4_get_tree(struct packed_git *p, struct pack_window **w_curs,
		   off_t obj_offset, unsigned long size);

enum decode_result {
	decode_failed = -1,	/* return value of error() */
	decode_one,
	decode_zero,
	decode_done
};

enum decoding_state {
	preparing,
	decoding,
	fallingback
};

struct pv4_tree_cache;

struct decode_state {
	struct decode_state *up;
	struct decode_state *free;

	/* Input */
	struct packed_git *p;
	struct pack_window **w_curs;
	off_t obj_offset; /* .. of the tree (after obj header) in the pack */
	unsigned int skip; /* ignore these many first tree entries */
	unsigned int count; /* number of tree entries to read, 0 = all */

	/* Output */
	unsigned int path_index;
	/*
	 * Can be converted back to index in sha1-table. If the
	 * pointer is not in sha-1 table, then it's in the current
	 * pack window and must be copied out because the pack window
	 * may move at the next decode_tree_entry()
	 */
	const unsigned char *sha1;

	int state;

	/* Packv4 data */
	const unsigned char *src; /* current reading position */
	unsigned long avail; /* don't read past src[0..avail-1] */
	/*
	 * index of the current tree entry, to see if we traversed
	 * full tree. If we don't, update tree-offset cache in case we
	 * need to walk this tree again in future.
	 */
	unsigned int curpos;
	unsigned int nb_entries; /* number of entries in this tree */
	off_t offset;		 /* position of "src" in the pack */
	off_t last_copy_base;
	struct pv4_tree_cache *cache;

	/* canonical tree */
	void *tree_v2;
	struct tree_desc desc;
};

int decode_tree_entry(struct decode_state **dsp);

#endif
