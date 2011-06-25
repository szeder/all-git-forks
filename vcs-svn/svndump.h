#ifndef SVNDUMP_H_
#define SVNDUMP_H_

struct svndump_options {
	/*
	 * dumpfile is opened in svndump_init and is read in svndump_read.
	 */
	const char *dumpfile, *git_svn_url;
	const char *ref;
};

int svndump_init(const struct svndump_options *o);
void svndump_read(void);
void svndump_deinit(void);
void svndump_reset(void);

#endif
