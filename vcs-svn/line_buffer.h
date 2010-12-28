#ifndef LINE_BUFFER_H_
#define LINE_BUFFER_H_

int buffer_init(const char *filename);
int buffer_deinit(void);
int buffer_ferror(void);
char *buffer_read_line(void);
char *buffer_read_string(uint32_t len);
/* Returns number of bytes read (not necessarily written). */
uint32_t buffer_copy_bytes(uint32_t len);
uint32_t buffer_skip_bytes(uint32_t len);
void buffer_reset(void);

#endif
