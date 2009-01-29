/*
 * rev-parse.c
 *
 * Copyright (C) Linus Torvalds, 2005
 */
#include "cache.h"
#include "commit.h"
#include "refs.h"
#include "quote.h"
#include "builtin.h"
#include "parse-options.h"

#define DO_REVS		1
#define DO_NOREV	2
#define DO_FLAGS	4
#define DO_NONFLAGS	8
static int filter = ~0;

static const char *def;

#define NORMAL 0
#define REVERSED 1
static int show_type = NORMAL;

#define SHOW_SYMBOLIC_ASIS 1
#define SHOW_SYMBOLIC_FULL 2
static int symbolic;
static int abbrev;
static int output_sq;

static int revs_count;

/*
 * Some arguments are relevant "revision" arguments,
 * others are about output format or other details.
 * This sorts it all out.
 */
static int is_rev_argument(const char *arg)
{
	static const char *rev_args[] = {
		"--all",
		"--bisect",
		"--dense",
		"--branches",
		"--header",
		"--max-age=",
		"--max-count=",
		"--min-age=",
		"--no-merges",
		"--objects",
		"--objects-edge",
		"--parents",
		"--pretty",
		"--remotes",
		"--sparse",
		"--tags",
		"--topo-order",
		"--date-order",
		"--unpacked",
		NULL
	};
	const char **p = rev_args;

	/* accept -<digit>, like traditional "head" */
	if ((*arg == '-') && isdigit(arg[1]))
		return 1;

	for (;;) {
		const char *str = *p++;
		int len;
		if (!str)
			return 0;
		len = strlen(str);
		if (!strcmp(arg, str) ||
		    (str[len-1] == '=' && !strncmp(arg, str, len)))
			return 1;
	}
}

/* Output argument as a string, either SQ or normal */
static void show(const char *arg)
{
	if (output_sq) {
		int sq = '\'', ch;

		putchar(sq);
		while ((ch = *arg++)) {
			if (ch == sq)
				fputs("'\\'", stdout);
			putchar(ch);
		}
		putchar(sq);
		putchar(' ');
	}
	else
		puts(arg);
}

/* Like show(), but with a negation prefix according to type */
static void show_with_type(int type, const char *arg)
{
	if (type != show_type)
		putchar('^');
	show(arg);
}

/* Output a revision, only if filter allows it */
static void show_rev(int type, const unsigned char *sha1, const char *name)
{
	if (!(filter & DO_REVS))
		return;
	def = NULL;
	revs_count++;

	if (symbolic && name) {
		if (symbolic == SHOW_SYMBOLIC_FULL) {
			unsigned char discard[20];
			char *full;

			switch (dwim_ref(name, strlen(name), discard, &full)) {
			case 0:
				/*
				 * Not found -- not a ref.  We could
				 * emit "name" here, but symbolic-full
				 * users are interested in finding the
				 * refs spelled in full, and they would
				 * need to filter non-refs if we did so.
				 */
				break;
			case 1: /* happy */
				show_with_type(type, full);
				break;
			default: /* ambiguous */
				error("refname '%s' is ambiguous", name);
				break;
			}
		} else {
			show_with_type(type, name);
		}
	}
	else if (abbrev)
		show_with_type(type, find_unique_abbrev(sha1, abbrev));
	else
		show_with_type(type, sha1_to_hex(sha1));
}

/* Output a flag, only if filter allows it. */
static int show_flag(const char *arg)
{
	if (!(filter & DO_FLAGS))
		return 0;
	if (filter & (is_rev_argument(arg) ? DO_REVS : DO_NOREV)) {
		show(arg);
		return 1;
	}
	return 0;
}

static void show_default(void)
{
	const char *s = def;

	if (s) {
		unsigned char sha1[20];

		def = NULL;
		if (!get_sha1(s, sha1)) {
			show_rev(NORMAL, sha1, s);
			return;
		}
	}
}

static int show_reference(const char *refname, const unsigned char *sha1, int flag, void *cb_data)
{
	show_rev(NORMAL, sha1, refname);
	return 0;
}

static void show_datestring(const char *flag, const char *datestr)
{
	static char buffer[100];

	/* date handling requires both flags and revs */
	if ((filter & (DO_FLAGS | DO_REVS)) != (DO_FLAGS | DO_REVS))
		return;
	snprintf(buffer, sizeof(buffer), "%s%lu", flag, approxidate(datestr));
	show(buffer);
}

static int show_file(const char *arg)
{
	show_default();
	if ((filter & (DO_NONFLAGS|DO_NOREV)) == (DO_NONFLAGS|DO_NOREV)) {
		show(arg);
		return 1;
	}
	return 0;
}

static int try_difference(const char *arg)
{
	char *dotdot;
	unsigned char sha1[20];
	unsigned char end[20];
	const char *next;
	const char *this;
	int symmetric;

	if (!(dotdot = strstr(arg, "..")))
		return 0;
	next = dotdot + 2;
	this = arg;
	symmetric = (*next == '.');

	*dotdot = 0;
	next += symmetric;

	if (!*next)
		next = "HEAD";
	if (dotdot == arg)
		this = "HEAD";
	if (!get_sha1(this, sha1) && !get_sha1(next, end)) {
		show_rev(NORMAL, end, next);
		show_rev(symmetric ? NORMAL : REVERSED, sha1, this);
		if (symmetric) {
			struct commit_list *exclude;
			struct commit *a, *b;
			a = lookup_commit_reference(sha1);
			b = lookup_commit_reference(end);
			exclude = get_merge_bases(a, b, 1);
			while (exclude) {
				struct commit_list *n = exclude->next;
				show_rev(REVERSED,
					 exclude->item->object.sha1,NULL);
				free(exclude);
				exclude = n;
			}
		}
		return 1;
	}
	*dotdot = '.';
	return 0;
}

static int parseopt_dump(const struct option *o, const char *arg, int unset)
{
	struct strbuf *parsed = o->value;
	if (unset)
		strbuf_addf(parsed, " --no-%s", o->long_name);
	else if (o->short_name)
		strbuf_addf(parsed, " -%c", o->short_name);
	else
		strbuf_addf(parsed, " --%s", o->long_name);
	if (arg) {
		strbuf_addch(parsed, ' ');
		sq_quote_buf(parsed, arg);
	}
	return 0;
}

static const char *skipspaces(const char *s)
{
	while (isspace(*s))
		s++;
	return s;
}

static int cmd_parseopt(int argc, const char **argv, const char *prefix)
{
	static int keep_dashdash = 0;
	static char const * const parseopt_usage[] = {
		"git-rev-parse --parseopt [options] -- [<args>...]",
		NULL
	};
	static struct option parseopt_opts[] = {
		OPT_BOOLEAN(0, "keep-dashdash", &keep_dashdash,
					"keep the `--` passed as an arg"),
		OPT_END(),
	};

	struct strbuf sb, parsed;
	const char **usage = NULL;
	struct option *opts = NULL;
	int onb = 0, osz = 0, unb = 0, usz = 0;

	strbuf_init(&parsed, 0);
	strbuf_addstr(&parsed, "set --");
	argc = parse_options(argc, argv, parseopt_opts, parseopt_usage,
	                     PARSE_OPT_KEEP_DASHDASH);
	if (argc < 1 || strcmp(argv[0], "--"))
		usage_with_options(parseopt_usage, parseopt_opts);

	strbuf_init(&sb, 0);
	/* get the usage up to the first line with a -- on it */
	for (;;) {
		if (strbuf_getline(&sb, stdin, '\n') == EOF)
			die("premature end of input");
		ALLOC_GROW(usage, unb + 1, usz);
		if (!strcmp("--", sb.buf)) {
			if (unb < 1)
				die("no usage string given before the `--' separator");
			usage[unb] = NULL;
			break;
		}
		usage[unb++] = strbuf_detach(&sb, NULL);
	}

	/* parse: (<short>|<short>,<long>|<long>)[=?]? SP+ <help> */
	while (strbuf_getline(&sb, stdin, '\n') != EOF) {
		const char *s;
		struct option *o;

		if (!sb.len)
			continue;

		ALLOC_GROW(opts, onb + 1, osz);
		memset(opts + onb, 0, sizeof(opts[onb]));

		o = &opts[onb++];
		s = strchr(sb.buf, ' ');
		if (!s || *sb.buf == ' ') {
			o->type = OPTION_GROUP;
			o->help = xstrdup(skipspaces(sb.buf));
			continue;
		}

		o->type = OPTION_CALLBACK;
		o->help = xstrdup(skipspaces(s));
		o->value = &parsed;
		o->flags = PARSE_OPT_NOARG;
		o->callback = &parseopt_dump;
		while (s > sb.buf && strchr("*=?!", s[-1])) {
			switch (*--s) {
			case '=':
				o->flags &= ~PARSE_OPT_NOARG;
				break;
			case '?':
				o->flags &= ~PARSE_OPT_NOARG;
				o->flags |= PARSE_OPT_OPTARG;
				break;
			case '!':
				o->flags |= PARSE_OPT_NONEG;
				break;
			case '*':
				o->flags |= PARSE_OPT_HIDDEN;
				break;
			}
		}

		if (s - sb.buf == 1) /* short option only */
			o->short_name = *sb.buf;
		else if (sb.buf[1] != ',') /* long option only */
			o->long_name = xmemdupz(sb.buf, s - sb.buf);
		else {
			o->short_name = *sb.buf;
			o->long_name = xmemdupz(sb.buf + 2, s - sb.buf - 2);
		}
	}
	strbuf_release(&sb);

	/* put an OPT_END() */
	ALLOC_GROW(opts, onb + 1, osz);
	memset(opts + onb, 0, sizeof(opts[onb]));
	argc = parse_options(argc, argv, opts, usage,
	                     keep_dashdash ? PARSE_OPT_KEEP_DASHDASH : 0);

	strbuf_addf(&parsed, " --");
	sq_quote_argv(&parsed, argv, 0);
	puts(parsed.buf);
	return 0;
}

int cmd_rev_parse(int argc, const char **argv, const char *prefix)
{
	int i, as_is = 0, verify = 0;
	unsigned char sha1[20];

	if (argc > 1 && !strcmp("--parseopt", argv[1]))
		return cmd_parseopt(argc - 1, argv + 1, prefix);

	prefix = setup_git_directory();
	git_config(git_default_config);
	for (i = 1; i < argc; i++) {
		const char *arg = argv[i];

		if (as_is) {
			if (show_file(arg) && as_is < 2)
				verify_filename(prefix, arg);
			continue;
		}
		if (!strcmp(arg,"-n")) {
			if (++i >= argc)
				die("-n requires an argument");
			if ((filter & DO_FLAGS) && (filter & DO_REVS)) {
				show(arg);
				show(argv[i]);
			}
			continue;
		}
		if (!prefixcmp(arg, "-n")) {
			if ((filter & DO_FLAGS) && (filter & DO_REVS))
				show(arg);
			continue;
		}

		if (*arg == '-') {
			if (!strcmp(arg, "--")) {
				as_is = 2;
				/* Pass on the "--" if we show anything but files.. */
				if (filter & (DO_FLAGS | DO_REVS))
					show_file(arg);
				continue;
			}
			if (!strcmp(arg, "--default")) {
				def = argv[i+1];
				i++;
				continue;
			}
			if (!strcmp(arg, "--revs-only")) {
				filter &= ~DO_NOREV;
				continue;
			}
			if (!strcmp(arg, "--no-revs")) {
				filter &= ~DO_REVS;
				continue;
			}
			if (!strcmp(arg, "--flags")) {
				filter &= ~DO_NONFLAGS;
				continue;
			}
			if (!strcmp(arg, "--no-flags")) {
				filter &= ~DO_FLAGS;
				continue;
			}
			if (!strcmp(arg, "--verify")) {
				filter &= ~(DO_FLAGS|DO_NOREV);
				verify = 1;
				continue;
			}
			if (!strcmp(arg, "--short") ||
			    !prefixcmp(arg, "--short=")) {
				filter &= ~(DO_FLAGS|DO_NOREV);
				verify = 1;
				abbrev = DEFAULT_ABBREV;
				if (arg[7] == '=')
					abbrev = strtoul(arg + 8, NULL, 10);
				if (abbrev < MINIMUM_ABBREV)
					abbrev = MINIMUM_ABBREV;
				else if (40 <= abbrev)
					abbrev = 40;
				continue;
			}
			if (!strcmp(arg, "--sq")) {
				output_sq = 1;
				continue;
			}
			if (!strcmp(arg, "--not")) {
				show_type ^= REVERSED;
				continue;
			}
			if (!strcmp(arg, "--symbolic")) {
				symbolic = SHOW_SYMBOLIC_ASIS;
				continue;
			}
			if (!strcmp(arg, "--symbolic-full-name")) {
				symbolic = SHOW_SYMBOLIC_FULL;
				continue;
			}
			if (!strcmp(arg, "--all")) {
				for_each_ref(show_reference, NULL);
				continue;
			}
			if (!strcmp(arg, "--branches")) {
				for_each_branch_ref(show_reference, NULL);
				continue;
			}
			if (!strcmp(arg, "--tags")) {
				for_each_tag_ref(show_reference, NULL);
				continue;
			}
			if (!strcmp(arg, "--remotes")) {
				for_each_remote_ref(show_reference, NULL);
				continue;
			}
			if (!strcmp(arg, "--show-prefix")) {
				if (prefix)
					puts(prefix);
				continue;
			}
			if (!strcmp(arg, "--show-cdup")) {
				const char *pfx = prefix;
				if (!is_inside_work_tree()) {
					const char *work_tree =
						get_git_work_tree();
					if (work_tree)
						printf("%s\n", work_tree);
					continue;
				}
				while (pfx) {
					pfx = strchr(pfx, '/');
					if (pfx) {
						pfx++;
						printf("../");
					}
				}
				putchar('\n');
				continue;
			}
			if (!strcmp(arg, "--git-dir")) {
				const char *gitdir = getenv(GIT_DIR_ENVIRONMENT);
				static char cwd[PATH_MAX];
				if (gitdir) {
					puts(gitdir);
					continue;
				}
				if (!prefix) {
					puts(".git");
					continue;
				}
				if (!getcwd(cwd, PATH_MAX))
					die("unable to get current working directory");
				printf("%s/.git\n", cwd);
				continue;
			}
			if (!strcmp(arg, "--is-inside-git-dir")) {
				printf("%s\n", is_inside_git_dir() ? "true"
						: "false");
				continue;
			}
			if (!strcmp(arg, "--is-inside-work-tree")) {
				printf("%s\n", is_inside_work_tree() ? "true"
						: "false");
				continue;
			}
			if (!strcmp(arg, "--is-bare-repository")) {
				printf("%s\n", is_bare_repository() ? "true"
						: "false");
				continue;
			}
			if (!prefixcmp(arg, "--since=")) {
				show_datestring("--max-age=", arg+8);
				continue;
			}
			if (!prefixcmp(arg, "--after=")) {
				show_datestring("--max-age=", arg+8);
				continue;
			}
			if (!prefixcmp(arg, "--before=")) {
				show_datestring("--min-age=", arg+9);
				continue;
			}
			if (!prefixcmp(arg, "--until=")) {
				show_datestring("--min-age=", arg+8);
				continue;
			}
			if (show_flag(arg) && verify)
				die("Needed a single revision");
			continue;
		}

		/* Not a flag argument */
		if (try_difference(arg))
			continue;
		if (!get_sha1(arg, sha1)) {
			show_rev(NORMAL, sha1, arg);
			continue;
		}
		if (*arg == '^' && !get_sha1(arg+1, sha1)) {
			show_rev(REVERSED, sha1, arg+1);
			continue;
		}
		as_is = 1;
		if (!show_file(arg))
			continue;
		if (verify)
			die("Needed a single revision");
		verify_filename(prefix, arg);
	}
	show_default();
	if (verify && revs_count != 1)
		die("Needed a single revision");
	return 0;
}
