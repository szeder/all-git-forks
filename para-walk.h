#ifndef PARA_WALK_H
#define PARA_WALK_H

struct para_walk_entry {
	const char *name;
	const unsigned char *hash;
	int namelen;
	unsigned mode;
};

struct para_iw {
	int pos;
	int slash;
};

struct para_tw_rec {
	struct tree_desc tree_desc;
	void *tree_buf;
	struct para_tw_rec *caller;
};

struct para_tw {
	struct para_tw_rec *current;
	char namebuf[PATH_MAX];
};

struct para_walk {
	const char **pathspec;
	int use_index;
	int use_worktree;
	int num_trees;
	/* 0: index, 1: worktree, 2: tree[0], 3: tree[1], ... */
	struct para_walk_entry *peek;
	struct para_iw iw;
	struct para_tw *tw;
	int pathlen;
	const char *path;
	char pathbuf[PATH_MAX];
};

/* extract and update return values */
#define WALKER_EOF (-1)
#define WALKER_ERR (-2)

int init_para_walk(struct para_walk *, const char **pathspec, int use_index, int use_worktree, int num_trees, unsigned char tree[][20]);
int extract_para_walk(struct para_walk *);
void update_para_walk(struct para_walk *);
void skip_para_walk(struct para_walk *);

#endif
