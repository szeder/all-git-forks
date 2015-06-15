/*
 * Another stupid program, this one parsing the headers of an
 * email to figure out authorship and subject
 */
#include "cache.h"
#include "builtin.h"
#include "strbuf.h"
#include "mailinfo.h"

static const char mailinfo_usage[] =
	"git mailinfo [-k | -b] [-m | --message-id] [-u | --encoding=<encoding> | -n] [--scissors | --no-scissors] <msg> <patch> < mail >info";

int cmd_mailinfo(int argc, const char **argv, const char *prefix)
{
	struct mailinfo_opts opts;
	const char *def_charset;

	/* NEEDSWORK: might want to do the optional .git/ directory
	 * discovery
	 */
	git_config(git_default_config, NULL);

	mailinfo_opts_init(&opts);

	def_charset = get_commit_output_encoding();

	while (1 < argc && argv[1][0] == '-') {
		if (!strcmp(argv[1], "-k"))
			opts.keep_subject = 1;
		else if (!strcmp(argv[1], "-b"))
			opts.keep_non_patch_brackets_in_subject = 1;
		else if (!strcmp(argv[1], "-m") || !strcmp(argv[1], "--message-id"))
			opts.add_message_id = 1;
		else if (!strcmp(argv[1], "-u"))
			opts.metainfo_charset = def_charset;
		else if (!strcmp(argv[1], "-n"))
			opts.metainfo_charset = NULL;
		else if (starts_with(argv[1], "--encoding="))
			opts.metainfo_charset = argv[1] + 11;
		else if (!strcmp(argv[1], "--scissors"))
			opts.use_scissors = 1;
		else if (!strcmp(argv[1], "--no-scissors"))
			opts.use_scissors = 0;
		else if (!strcmp(argv[1], "--no-inbody-headers"))
			opts.use_inbody_headers = 0;
		else
			usage(mailinfo_usage);
		argc--; argv++;
	}

	if (argc != 3)
		usage(mailinfo_usage);

	return !!mailinfo(&opts, stdin, stdout, argv[1], argv[2]);
}
