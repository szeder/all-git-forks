#include "cache.h"
#include "submodule.h"
#include "dir.h"
#include "diff.h"
#include "commit.h"
#include "revision.h"
#include "run-command.h"
#include "diffcore.h"
#include "refs.h"
#include "string-list.h"


int cmd_heiko(int argc, const char **argv2, const char *prefix)
{
	int is_present = 0;
	struct child_process cp;
	const char *argv[] = {"rev-list", "-n", "1", "HEAD", NULL};
	struct strbuf buf = STRBUF_INIT;

	memset(&cp, 0, sizeof(cp));
	cp.argv = argv;
	cp.env = local_repo_env;
	cp.git_cmd = 1;
	cp.no_stdin = 1;
	cp.out = -1;
	if (!run_command(&cp) && !strbuf_read(&buf, cp.out, 1024))
		is_present = 1;

	fprintf(stderr, "len: %zu\n", (int) buf.len);
	close(cp.out);
	strbuf_release(&buf);

	return 0;
}
