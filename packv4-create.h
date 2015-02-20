#ifndef PACKV4_CREATE_H
#define PACKV4_CREATE_H

struct packv4_tables {
	struct pack_idx_entry *all_objs;
	unsigned all_objs_nr;
	struct dict_table *commit_ident_table;
	struct dict_table *tree_path_table;
};

struct packv4_dict;
struct dict_table;
struct sha1file;

struct dict_table *create_dict_table(void);
int dict_add_entry(struct dict_table *t, int val, const char *str, int str_len);
struct dict_table *pv4_dict_to_dict_table(struct packv4_dict *dict);
void destroy_dict_table(struct dict_table *t);
void dict_dump(struct packv4_tables *v4);

int add_commit_dict_entries(struct dict_table *commit_ident_table,
			    void *buf, unsigned long size);
int add_tree_dict_entries(struct dict_table *tree_path_table,
			  void *buf, unsigned long size);
void sort_dict_entries_by_hits(struct dict_table *t);

int encode_sha1ref(const struct packv4_tables *v4,
		   const unsigned char *sha1, unsigned char *buf);
unsigned long packv4_write_tables(struct sha1file *f,
				  const struct packv4_tables *v4,
				  int pack_compression_level);
void *pv4_encode_commit(const struct packv4_tables *v4,
			void *buffer, unsigned long *sizep,
			int pack_compression_level);
void *pv4_encode_tree(const struct packv4_tables *v4,
		      void *_buffer, unsigned long *sizep,
		      void *delta, unsigned long delta_size,
		      const unsigned char *delta_sha1);

struct unpacked;
struct tree_data;
struct copy_sequence;
struct encode_data {
	struct tree_data *tree;
	const struct packv4_tables *v4;
	int nr, nb_entries, max_depth;
	struct copy_sequence *seq;
	int seq_nr, seq_alloc;
	struct copy_sequence **copy_map;
};

void pv4_encode_tree_start(struct encode_data *ed,
			   const struct packv4_tables *v4,
			   unsigned idx,
			   struct unpacked *array, unsigned window,
			   int max_depth, unsigned long *mem_usage);
void pv4_encode_tree_finish(struct encode_data *ed);

#endif
