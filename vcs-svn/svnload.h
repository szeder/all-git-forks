#ifndef SVNLOAD_H_
#define SVNLOAD_H_

#include "strbuf.h"

#define SVN_DATE_FORMAT "%Y-%m-%dT%H:%M:%S.000000Z"
#define SVN_DATE_LEN 28
#define COPY_BUFFER_LEN 4096

struct ident {
	struct strbuf name, email;
	char date[SVN_DATE_LEN];
};

int svnload_init(const char *filename);
void svnload_deinit(void);
void svnload_read(void);

#endif
