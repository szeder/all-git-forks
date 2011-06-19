#ifndef SVNDUMP_H_
#define SVNDUMP_H_

struct svndump_args {
	const char *filename, *url;
};

int svndump_init(const struct svndump_args *args);
void svndump_read(void);
void svndump_deinit(void);
void svndump_reset(void);

#endif
