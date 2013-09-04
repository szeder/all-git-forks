#include "cache.h"
#include "refs.h"
#include "builtin.h"
#include "parse-options.h"
#include "quote.h"
#include "argv-array.h"

static const char * const git_update_ref_usage[] = {
	N_("git update-ref [options] -d <refname> [<oldval>]"),
	N_("git update-ref [options]    <refname> <newval> [<oldval>]"),
	N_("git update-ref [options] --stdin [-z]"),
	NULL
};

static int updates_alloc;
static int updates_count;
static const struct ref_update **updates;

static void update_refs_stdin(int argc, const char **argv)
{
	struct ref_update *update;

	/* Skip blank lines */
	if (!argc)
		return;

	/* Allocate and zero-init a struct ref_update */
	update = xcalloc(1, sizeof(*update));
	ALLOC_GROW(updates, updates_count+1, updates_alloc);
	updates[updates_count++] = update;

	/* Process options */
	while (argc > 0 && argv[0][0] == '-') {
		const char *arg = argv[0];
		--argc;
		++argv;
		if (!strcmp(arg, "--no-deref"))
			update->flags |= REF_NODEREF;
		else if (!strcmp(arg, "--"))
			break;
		else
			die("unknown option %s", arg);
	}

	/* Set the update ref_name */
	if (argc < 1)
		die("input line with no ref!");
	if (check_refname_format(argv[0], REFNAME_ALLOW_ONELEVEL))
		die("invalid ref format: %s", argv[0]);
	update->ref_name = xstrdup(argv[0]);

	/* Set the update new_sha1 and, if specified, old_sha1 */
	if (argc < 2)
		die("missing new value for ref %s", update->ref_name);
	if (*argv[1] && get_sha1(argv[1], update->new_sha1))
		die("invalid new value for ref %s: %s",
		    update->ref_name, argv[1]);
	if (argc >= 3) {
		update->have_old = 1;
		if (*argv[2] && get_sha1(argv[2], update->old_sha1))
			die("invalid old value for ref %s: %s",
			    update->ref_name, argv[2]);
	}

	if (argc > 3)
		die("too many arguments for ref %s", update->ref_name);
}

static const char *update_refs_stdin_parse_arg(const char *next,
					       struct strbuf *arg)
{
	/* Skip leading whitespace */
	while (isspace(*next))
		++next;

	/* Return NULL when no argument is found */
	if (!*next)
		return NULL;

	/* Parse the argument */
	strbuf_reset(arg);
	if (*next == '"') {
		if (unquote_c_style(arg, next, &next))
			die("badly quoted argument: %s", next);
		return next;
	}
	while (*next && !isspace(*next))
		strbuf_addch(arg, *next++);
	return next;
}

static void update_refs_stdin_parse_line(const char *next)
{
	struct strbuf arg = STRBUF_INIT;
	static struct argv_array args = ARGV_ARRAY_INIT;

	/* Parse arguments on this line */
	while ((next = update_refs_stdin_parse_arg(next, &arg)) != NULL)
		argv_array_push(&args, arg.buf);

	/* Process this command */
	update_refs_stdin(args.argc, args.argv);

	argv_array_clear(&args);
	strbuf_release(&arg);
}

static void update_refs_stdin_read_n(void)
{
	struct strbuf line = STRBUF_INIT;

	while (strbuf_getline(&line, stdin, '\n') != EOF)
		update_refs_stdin_parse_line(line.buf);

	strbuf_release(&line);
}

static void update_refs_stdin_read_z(void)
{
	struct strbuf arg = STRBUF_INIT;
	static struct argv_array args = ARGV_ARRAY_INIT;

	/* Process NUL-terminated arguments with commands ending in LF */
	while (strbuf_getline(&arg, stdin, '\0') != EOF) {
		if (!strcmp(arg.buf, "\n")) {
			update_refs_stdin(args.argc, args.argv);
			argv_array_clear(&args);
		} else {
			argv_array_push(&args, arg.buf);
		}
	}

	if (args.argc > 0)
		die("unterminated -z input sequence");

	strbuf_release(&arg);
}

int cmd_update_ref(int argc, const char **argv, const char *prefix)
{
	const char *refname, *oldval, *msg = NULL;
	unsigned char sha1[20], oldsha1[20];
	int delete = 0, no_deref = 0, read_stdin = 0, end_null = 0, flags = 0;
	struct option options[] = {
		OPT_STRING( 'm', NULL, &msg, N_("reason"), N_("reason of the update")),
		OPT_BOOLEAN('d', NULL, &delete, N_("delete the reference")),
		OPT_BOOLEAN('z', NULL, &end_null, N_("stdin has NUL-terminated arguments")),
		OPT_BOOLEAN( 0 , "no-deref", &no_deref,
					N_("update <refname> not the one it points to")),
		OPT_BOOLEAN( 0 , "stdin", &read_stdin, N_("read updates from standard input")),
		OPT_END(),
	};

	git_config(git_default_config, NULL);
	argc = parse_options(argc, argv, prefix, options, git_update_ref_usage,
			     0);
	if (msg && !*msg)
		die("Refusing to perform update with empty message.");

	if (read_stdin) {
		if (delete || no_deref || argc > 0)
			usage_with_options(git_update_ref_usage, options);
		if (end_null)
			update_refs_stdin_read_z();
		else
			update_refs_stdin_read_n();
		return update_refs(msg, updates, updates_count, DIE_ON_ERR);
	}

	if (end_null)
		usage_with_options(git_update_ref_usage, options);

	if (delete) {
		if (argc < 1 || argc > 2)
			usage_with_options(git_update_ref_usage, options);
		refname = argv[0];
		oldval = argv[1];
	} else {
		const char *value;
		if (argc < 2 || argc > 3)
			usage_with_options(git_update_ref_usage, options);
		refname = argv[0];
		value = argv[1];
		oldval = argv[2];
		if (get_sha1(value, sha1))
			die("%s: not a valid SHA1", value);
	}

	hashclr(oldsha1); /* all-zero hash in case oldval is the empty string */
	if (oldval && *oldval && get_sha1(oldval, oldsha1))
		die("%s: not a valid old SHA1", oldval);

	if (no_deref)
		flags = REF_NODEREF;
	if (delete)
		return delete_ref(refname, oldval ? oldsha1 : NULL, flags);
	else
		return update_ref(msg, refname, sha1, oldval ? oldsha1 : NULL,
				  flags, DIE_ON_ERR);
}
