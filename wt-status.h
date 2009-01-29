#ifndef STATUS_H
#define STATUS_H

#include <stdio.h>

enum color_wt_status {
	WT_STATUS_HEADER,
	WT_STATUS_UPDATED,
	WT_STATUS_CHANGED,
	WT_STATUS_UNTRACKED,
};

struct wt_status {
	int is_initial;
	char *branch;
	const char *reference;
	int verbose;
	int amend;
	int untracked;
	int nowarn;
	/* These are computed during processing of the individual sections */
	int commitable;
	int workdir_dirty;
	int workdir_untracked;
	const char *index_file;
	FILE *fp;
	const char *prefix;
};

int git_status_config(const char *var, const char *value);
extern int wt_status_use_color;
extern int wt_status_relative_paths;
void wt_status_prepare(struct wt_status *s);
void wt_status_print(struct wt_status *s);

#endif /* STATUS_H */
