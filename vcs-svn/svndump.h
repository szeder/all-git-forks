#ifndef SVNDUMP_H_
#define SVNDUMP_H_

int svndump_init(const char *filename, const char *url, const char *dst_ref, int report_fileno, int progress);
void svndump_read(void);
void svndump_deinit(void);
void svndump_reset(void);

#endif
