#ifndef WATCHMAN_SUPPORT_H
#define WATCHMAN_SUPPORT_H

struct index_state;
int check_watchman(struct index_state *index);

void check_run_watchman(void);

#endif /* WATCHMAN_SUPPORT_H */
