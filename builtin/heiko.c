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

static int handle_remote_submodule_ref(const char *refname,
		const unsigned char *sha1, int flags, void *cb_data)
{
	struct strbuf *output = cb_data;

	strbuf_addf(output, "%s\n", refname);

	return 0;
}

int cmd_heiko(int argc, const char **argv2, const char *prefix)
{
	struct strbuf output = STRBUF_INIT;
	const char *submodule;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <submodule>", argv2[0]);
		return 1;
	}

	submodule = argv2[1];

	if (add_submodule_odb(submodule)) {
		fprintf(stderr, "Error submodule '%s' not populated.",
				submodule);
		return 1;
	}
	for_each_remote_ref_submodule(submodule, handle_remote_submodule_ref,
			&output);

	printf("%s", output.buf);
	return 0;
}
