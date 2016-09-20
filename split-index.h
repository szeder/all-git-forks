#ifndef SPLIT_INDEX_H
#define SPLIT_INDEX_H

struct index_state;
struct strbuf;
struct ewah_bitmap;

struct split_index {
	unsigned char base_sha1[20];
	struct index_state *base;
	struct ewah_bitmap *delete_bitmap;
	struct ewah_bitmap *replace_bitmap;
	struct cache_entry **saved_cache;
	unsigned int saved_cache_nr;
	unsigned int nr_deletions;
	unsigned int nr_replacements;
	int refcount;
};

struct split_index *init_split_index(struct index_state *istate);
void save_or_free_index_entry(struct index_state *istate, struct cache_entry *ce);
void replace_index_entry_in_base(struct index_state *istate,
				 struct cache_entry *old,
				 struct cache_entry *new);
int read_link_extension(struct index_state *istate,
			const void *data, unsigned long sz);
int write_link_extension(struct strbuf *sb,
			 struct index_state *istate);
void move_cache_to_base_index(struct index_state *istate);
void merge_base_index(struct index_state *istate);
void prepare_to_write_split_index(struct index_state *istate);
void finish_writing_split_index(struct index_state *istate);
void discard_split_index(struct index_state *istate);
void add_split_index(struct index_state *istate);
void remove_split_index(struct index_state *istate);

/*
 * Canary files are files designed to help decide if it's ok to delete
 * share index files.
 *
 * A shared index file should be deleted only if is not refered by any
 * split-index file. The problem is that there is currently no way to
 * know where are all the split-index files that could reference a
 * shared index file. The purpose of canary files is to help track
 * this information.
 *
 * A canary file should have a filename that is a concatenation of the
 * shared index sha1 (si->base->sha1) and the sha1 hash of the path of
 * the split-index that will reference the shared index.
 *
 * This makes sure that a canary file is specific to a split-index
 * file and that it is easy to find all the canary files related to a
 * specific shared index file.
 *
 * To simplify things let's create the canary file just before it's
 * split-index file is renamed to it's final name.
 *
 * => Q: what happens if we check whether we can delete a shared index
 * file between the time when the shared index file has been created
 * and the time the canary file is created?
 *
 * => A: we can avoid that problem by requiring that only shared index
 * files older than one day can be deleted (if they also have no
 * corresponding canary file).
 *
 * => Q: When we are creating a new split-index file how do we know
 * that the referenced shared index file is not being deleted?
 *
 * => A: We can avoid that by "touch"ing the shared index file before
 * creating the split-index file.
 */

void split_index_canary_filename(struct strbuf *sb,
				 const char *shared_index,
				 const char *path);
void write_split_index_canary(const char *shared_index, const char *path);

#endif
