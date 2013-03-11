#ifndef META_H
#define META_H

#include "hash.h"

unsigned int hash_path(const char *path);
const char *get_meta_ref_prefix();

void set_ref_prefix(const char *);
const char *get_ref_prefix();
/*
 * struct represents a revision of a single file in CVS.
 * patchset field is always initialized and used
 * as author and msg placeholder, it may be changed
 * later during patch aggregation
 */
struct file_revision {
	//struct file_revision *hash_tbl_chain;
	char *path;
	char *revision;
	int ismeta:1;
	int isdead:1;
	int ismerged:1;
	struct file_revision *prev;
	//struct file_revision *next;
	struct patchset *patchset;
	time_t timestamp;
};

/*
 * smaller version of a struct file_revision that is used to
 * load file revisions information from metadata
 */
struct file_revision_meta {
	//struct file_revision *hash_tbl_chain;
	char *path;
	char *revision;
	int ismeta:1;
	int isdead:1;
};

struct file_revision_list {
	unsigned int size, nr;
	struct file_revision **item;
};

struct patchset {
	time_t timestamp;
	time_t timestamp_last;
	char *author;
	char *msg;
	struct patchset *next; // patchset list <--- will be sorted by timestamp
	struct hash_table *revision_hash; // path -> file_revision hash

	time_t cancellation_point; // non zero if patchset is save cancellation point,
				   // timestamp has to be used for next cvs update
};

struct patchset_list {
	struct patchset *head;
	struct patchset *tail;
};

/*
 * struct branch_meta represents a branch in CVS
 * with all metadata needed for patch aggregation
 * and consistency validation
 */
struct branch_meta {
	//TODO: branch name
	struct hash_table *patchset_hash; // author_msg2ps -> patchset hash
	struct hash_table *revision_hash; // path -> file_revision hash
	struct file_revision_list *rev_list;
	struct patchset_list *patchset_list;

	struct hash_table *last_commit_revision_hash; // path -> file_revision_meta hash
	time_t last_revision_timestamp;

	unsigned int fuzz_time;
};

/*
 * main cmd parses options, parses refs, gives git branch ref and cvs branch
 */

struct branch_meta *new_branch_meta(const char *branch_name);
void add_file_revision(struct branch_meta *meta,
		       const char *path,
		       const char *revision,
		       const char *author,
		       const char *msg,
		       time_t timestamp,
		       int isdead);

void aggregate_patchsets(struct branch_meta *meta);
void free_branch_meta(struct branch_meta *meta);

/*
 * branch name to meta map
 */
struct meta_map_entry {
	char *branch_name;
	struct branch_meta *meta;
};

struct meta_map {
	unsigned int size, nr;
	struct meta_map_entry *array;
};

#define for_each_branch_meta(item,map) \
	for (item = (map)->array; item < (map)->array + (map)->nr; ++item)

void meta_map_init(struct meta_map *map);
void meta_map_add(struct meta_map *map, const char *branch_name, struct branch_meta *meta);
struct branch_meta *meta_map_find(struct meta_map *map, const char *branch_name);
void meta_map_release(struct meta_map *map);

/*
 * metadata work
 */
int load_cvs_revision_meta(struct branch_meta *meta,
			   const char *commit_ref,
			   const char *notes_ref);

int save_cvs_revision_meta(struct branch_meta *meta,
			   const char *commit_ref,
			   const char *notes_ref);

#endif
