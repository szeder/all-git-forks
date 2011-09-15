/*
 * test-svn-fe: Code to exercise the svn import lib
 */

#include "git-compat-util.h"
#include "parse-options.h"
#include "vcs-svn/svndump.h"
#include "vcs-svn/svndiff.h"
#include "vcs-svn/sliding_window.h"
#include "vcs-svn/line_buffer.h"

static const char * const test_svnfe_usage[] = {
	"test-svn-fe [options] <dumpfile>",
	"test-svn-fe -d <preimage> <delta> <len>",
	NULL
};

static struct svndump_options options;

static int delta_test;

static struct option test_svnfe_options[] = {
	OPT_SET_INT('d', "apply-delta", &delta_test, "test apply_delta", 1),
	OPT_STRING(0, "ref", &options.ref, "refname",
		"write to <refname> instead of refs/heads/master"),
	OPT_SET_INT(0, "incremental", &options.incremental,
		"resume export, requires marks and incremental dump",
		1),
	OPT_INTEGER(0, "read-blob-fd", &options.backflow_fd,
		"read blobs and trees from this fd instead of 3"),
	OPT_END()
};

static int apply_delta(int argc, const char *argv[])
{
	struct line_buffer preimage = LINE_BUFFER_INIT;
	struct line_buffer delta = LINE_BUFFER_INIT;
	struct sliding_view preimage_view = SLIDING_VIEW_INIT(&preimage, -1);

	if (argc != 3)
		usage_with_options(test_svnfe_usage, test_svnfe_options);

	if (buffer_init(&preimage, argv[0]))
		die_errno("cannot open preimage");
	if (buffer_init(&delta, argv[1]))
		die_errno("cannot open delta");
	if (svndiff0_apply(&delta, (off_t) strtoull(argv[2], NULL, 0),
					&preimage_view, stdout))
		return 1;
	if (buffer_deinit(&preimage))
		die_errno("cannot close preimage");
	if (buffer_deinit(&delta))
		die_errno("cannot close delta");
	buffer_reset(&preimage);
	strbuf_release(&preimage_view.buf);
	buffer_reset(&delta);
	return 0;
}

int main(int argc, const char *argv[])
{
	options.backflow_fd = 3;
	options.progress = 1;
	argc = parse_options(argc, argv, NULL, test_svnfe_options,
						test_svnfe_usage, 0);

	if (delta_test)
		return apply_delta(argc, argv);

	if (argc == 1) {
		options.dumpfile = argv[0];
		if (svndump_init(&options))
			return 1;
		svndump_read();
		svndump_deinit();
		svndump_reset();
		return 0;
	}

	usage_with_options(test_svnfe_usage, test_svnfe_options);
}
