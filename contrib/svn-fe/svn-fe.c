/*
 * This file is in the public domain.
 * You may freely use, modify, distribute, and relicense it.
 */

#include "git-compat-util.h"
#include "parse-options.h"
#include "svndump.h"

static const char * const svn_fe_usage[] = {
	"svn-fe [options] [git-svn-id-url] < dump | fast-import-backend",
	NULL
};

static const char *url;
static const char *ref = "refs/heads/master";
static int backflow_fd = 3;
static int progress = 1;

static struct option svn_fe_options[] = {
	OPT_BIT(0, "progress", &progress,
		"write a progress line after each commit",
		1),
	OPT_STRING(0, "git-svn-id-url", &url, "url",
		"append git-svn metadata line to commit messages"),
	OPT_STRING(0, "ref", &ref, "dst_ref",
		"write to dst_ref instead of refs/heads/master"),
	OPT_INTEGER(0, "read-blob-fd", &backflow_fd,
		"read blobs and trees from this fd instead of 3"),
	OPT_END()
};

int main(int argc, const char **argv)
{
	argc = parse_options(argc, argv, NULL, svn_fe_options,
						svn_fe_usage, 0);
	if (argc == 1) {
		if (url)
			usage_msg_opt("git-svn-id-url is set twice: as a "
					"--parameter and as a [parameter]",
					svn_fe_usage, svn_fe_options);
		url = argv[0];
	} else if (argc)
		usage_with_options(svn_fe_usage, svn_fe_options);
	if (svndump_init(NULL, url, ref, backflow_fd, progress))
		return 1;
	svndump_read();
	svndump_deinit();
	svndump_reset();
	return 0;
}
