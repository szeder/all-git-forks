#ifndef PROGRESS_H
#define PROGRESS_H

struct progress;

void update_progress_total(struct progress *progress, unsigned total);
void update_progress_title(struct progress *progress, const char *title);

void show_progress_count(struct progress *progress, int flag);

void display_throughput(struct progress *progress, off_t total);
int display_progress(struct progress *progress, unsigned n);
struct progress *start_progress(const char *title, unsigned total);
struct progress *start_progress_delay(const char *title, unsigned total,
				       unsigned percent_treshold, unsigned delay);
void stop_progress(struct progress **progress);
void stop_progress_msg(struct progress **progress, const char *msg);

#endif
