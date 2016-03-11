#ifndef JOURNAL_CLIENT_H
#define JOURNAL_CLIENT_H

#include "cache.h"
#include "remote.h"
#include "journal.h"

struct lock_file;

struct remote_state {
	struct remote_state_rec r;
	struct journal_extent_rec last;
	size_t rec_count;
	struct lock_file *lock;
};

const char *extents_path(struct remote *upstream);
const char *remote_state_path(struct remote *upstream);
int extents_current_state(struct remote *upstream, struct remote_state *r, int verbose);
void read_extent_rec(int ext_fd, struct journal_extent_rec *rec);
void remote_state_load(struct remote *upstream, struct remote_state * const state, int verbose);
void remote_state_store(struct remote *upstream, struct remote_state *state, int close);
void remote_name_sanity_check(const char *remote_name);
int journal_present(void);
#endif /* JOURNAL_CLIENT_H */
