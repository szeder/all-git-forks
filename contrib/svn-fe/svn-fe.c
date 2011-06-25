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

static struct svndump_options options;

static struct option svn_fe_options[] = {
	{ OPTION_SET_INT, 0, "progress", &options.progress,
		NULL, "don't write a progress line after each commit",
		PARSE_OPT_NOARG | PARSE_OPT_NEGHELP, NULL, 1 },
	OPT_STRING(0, "git-svn-id-url", &options.git_svn_url, "url",
		"add git-svn-id line to log messages, imitating git-svn"),
	OPT_STRING(0, "ref", &options.ref, "refname",
		"write to <refname> instead of refs/heads/master"),
	OPT_INTEGER(0, "read-blob-fd", &options.backflow_fd,
		"read blobs and trees from this fd instead of 3"),
	OPT_END()
};

int main(int argc, const char **argv)
{
	options.backflow_fd = 3;
	argc = parse_options(argc, argv, NULL, svn_fe_options,
						svn_fe_usage, 0);
	if (argc > 1)
		usage_with_options(svn_fe_usage, svn_fe_options);

	if (argc == 1) {
		if (options.git_svn_url)
			usage_msg_opt("git-svn-id-url is set twice: as a "
					"--parameter and as a [parameter]",
					svn_fe_usage, svn_fe_options);
		options.git_svn_url = argv[0];
	}
	if (svndump_init(&options))
		return 1;
	svndump_read();
	svndump_deinit();
	svndump_reset();
	return 0;
}
