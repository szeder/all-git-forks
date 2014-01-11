#ifndef __FILE_WATCHER_LIB__
#define __FILE_WATCHER_LIB__

void open_watcher(struct index_state *istate);
void watch_entries(struct index_state *istate);
void close_watcher(struct index_state *istate, const unsigned char *sha1);

#endif
