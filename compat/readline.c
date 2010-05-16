#include "../git-compat-util.h"
#include "../strbuf.h"

char *gitreadline(const char *prompt)
{
	char *ret;
	struct strbuf buf = STRBUF_INIT;

	fputs(prompt, stdout);

	for (;;) {
		int c = getchar();
		if (c == EOF) {
			strbuf_release(&buf);
			return NULL;
		}
		if (c == '\n')
			break;
		if (c != '\r')
			strbuf_addch(&buf, c);
	}
	ret = strbuf_detach(&buf, NULL);
	if (!ret)
		return xstrdup("");
	return ret;
}
