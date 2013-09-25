#include "builtin.h"
#include "cache.h"
#include "commit.h"
#include "string-list.h"
#include "revision.h"
#include "shortlog.h"
#include "parse-options.h"

static char const * const shortlog_usage[] = {
	N_("git shortlog [<options>] [<revision range>] [[--] [<path>...]]"),
	NULL
};

static void read_from_stdin(struct shortlog *log)
{
	char author[1024], oneline[1024];

	while (fgets(author, sizeof(author), stdin) != NULL) {
		if (!(author[0] == 'A' || author[0] == 'a') ||
		    prefixcmp(author + 1, "uthor: "))
			continue;
		while (fgets(oneline, sizeof(oneline), stdin) &&
		       oneline[0] != '\n')
			; /* discard headers */
		while (fgets(oneline, sizeof(oneline), stdin) &&
		       oneline[0] == '\n')
			; /* discard blanks */
		shortlog_insert_one_record(log, author + 8, oneline);
	}
}

static void get_from_rev(struct rev_info *rev, struct shortlog *log)
{
	struct commit *commit;

	if (prepare_revision_walk(rev))
		die(_("revision walk setup failed"));
	while ((commit = get_revision(rev)) != NULL)
		shortlog_add_commit(log, commit);
}

static int parse_uint(char const **arg, int comma, int defval)
{
	unsigned long ul;
	int ret;
	char *endp;

	ul = strtoul(*arg, &endp, 10);
	if (*endp && *endp != comma)
		return -1;
	if (ul > INT_MAX)
		return -1;
	ret = *arg == endp ? defval : (int)ul;
	*arg = *endp ? endp + 1 : endp;
	return ret;
}

static const char wrap_arg_usage[] = "-w[<width>[,<indent1>[,<indent2>]]]";

static int parse_wrap_args(const struct option *opt, const char *arg, int unset)
{
	struct shortlog *log = opt->value;

	log->wrap_lines = !unset;
	if (unset)
		return 0;
	if (!arg) {
		log->wrap = DEFAULT_WRAPLEN;
		log->in1 = DEFAULT_INDENT1;
		log->in2 = DEFAULT_INDENT2;
		return 0;
	}

	log->wrap = parse_uint(&arg, ',', DEFAULT_WRAPLEN);
	log->in1 = parse_uint(&arg, ',', DEFAULT_INDENT1);
	log->in2 = parse_uint(&arg, '\0', DEFAULT_INDENT2);
	if (log->wrap < 0 || log->in1 < 0 || log->in2 < 0)
		return error(wrap_arg_usage);
	if (log->wrap &&
	    ((log->in1 && log->wrap <= log->in1) ||
	     (log->in2 && log->wrap <= log->in2)))
		return error(wrap_arg_usage);
	return 0;
}

int cmd_shortlog(int argc, const char **argv, const char *prefix)
{
	static struct shortlog log;
	static struct rev_info rev;
	int nongit = !startup_info->have_repository;

	static const struct option options[] = {
		OPT_BOOL('n', "numbered", &log.sort_by_number,
			 N_("sort output according to the number of commits per author")),
		OPT_BOOL('s', "summary", &log.summary,
			 N_("Suppress commit descriptions, only provides commit count")),
		OPT_BOOL('e', "email", &log.email,
			 N_("Show the email address of each author")),
		{ OPTION_CALLBACK, 'w', NULL, &log, N_("w[,i1[,i2]]"),
			N_("Linewrap output"), PARSE_OPT_OPTARG, &parse_wrap_args },
		OPT_END(),
	};

	struct parse_opt_ctx_t ctx;

	git_config(git_default_config, NULL);
	shortlog_init(&log);
	init_revisions(&rev, prefix);
	parse_options_start(&ctx, argc, argv, prefix, options,
			    PARSE_OPT_KEEP_DASHDASH | PARSE_OPT_KEEP_ARGV0);

	for (;;) {
		switch (parse_options_step(&ctx, options, shortlog_usage)) {
		case PARSE_OPT_HELP:
			exit(129);
		case PARSE_OPT_DONE:
			goto parse_done;
		}
		parse_revision_opt(&rev, &ctx, options, shortlog_usage);
	}
parse_done:
	argc = parse_options_end(&ctx);

	if (setup_revisions(argc, argv, &rev, NULL) != 1) {
		error(_("unrecognized argument: %s"), argv[1]);
		usage_with_options(shortlog_usage, options);
	}

	log.user_format = rev.commit_format == CMIT_FMT_USERFORMAT;
	log.abbrev = rev.abbrev;

	/* assume HEAD if from a tty */
	if (!nongit && !rev.pending.nr && isatty(0))
		add_head_to_pending(&rev);
	if (rev.pending.nr == 0) {
		if (isatty(0))
			fprintf(stderr, _("(reading log message from standard input)\n"));
		read_from_stdin(&log);
	}
	else
		get_from_rev(&rev, &log);

	shortlog_output(&log);
	return 0;
}
