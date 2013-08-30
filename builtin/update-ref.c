#include "cache.h"
#include "refs.h"
#include "builtin.h"
#include "parse-options.h"

static const char * const git_update_ref_usage[] = {
	N_("git update-ref [options] -d <refname> [<oldval>]"),
	N_("git update-ref [options]    <refname> <newval> [<oldval>]"),
	N_("git update-ref [options] --stdin"),
	NULL
};

static const char blank[] = " \t\r\n";

static int updates_size;
static int updates_count;
static struct ref_update *updates;

static const char* update_refs_stdin_next_arg(const char* next,
					      struct strbuf *arg)
{
	/* Skip leading whitespace: */
	while (isspace(*next))
		++next;

	/* Return NULL when no argument is found: */
	if (!*next)
		return NULL;

	/* Parse the argument: */
	strbuf_reset(arg);
	for (;;) {
		char c = *next;
		if (!c || isspace(c))
			break;
		++next;
		if (c == '\'') {
			size_t len = strcspn(next, "'");
			if (!next[len])
				die("unterminated single-quote: '%s", next);
			strbuf_add(arg, next, len);
			next += len + 1;
			continue;
		}
		if (c == '\\') {
			if (*next == '\'')
				c = *next++;
			else
				die("unquoted backslash not escaping "
				    "single-quote: \\%s", next);
		}
		strbuf_addch(arg, c);
	}
	return next;
}

static void update_refs_stdin(const char *line)
{
	int options = 1, flags = 0, argc = 0;
	char *argv[3] = {0, 0, 0};
	struct strbuf arg = STRBUF_INIT;
	struct ref_update *update;
	const char *next = line;

	/* Skip blank lines: */
	if (!line[0])
		return;

	/* Parse arguments on this line: */
	while ((next = update_refs_stdin_next_arg(next, &arg)) != NULL) {
		if (options && arg.buf[0] == '-')
			if (!strcmp(arg.buf, "--no-deref"))
				flags |= REF_NODEREF;
			else if (!strcmp(arg.buf, "--"))
				options = 0;
			else
				die("unknown option %s", arg.buf);
		else if (argc >= 3)
			die("too many arguments on line: %s", line);
		else {
			argv[argc++] = xstrdup(arg.buf);
			options = 0;
		}
	}
	strbuf_release(&arg);

	/* Allocate and zero-init a struct ref_update: */
	if (updates_count == updates_size) {
		updates_size = updates_size ? (updates_size * 2) : 16;
		updates = xrealloc(updates, sizeof(*updates) * updates_size);
		memset(updates + updates_count, 0,
		       sizeof(*updates) * (updates_size - updates_count));
	}
	update = &updates[updates_count++];
	update->flags = flags;

	/* Set the update ref_name: */
	if (!argv[0])
		die("no ref on line: %s", line);
	if (check_refname_format(argv[0], REFNAME_ALLOW_ONELEVEL))
		die("invalid ref format on line: %s", line);
	update->ref_name = argv[0];
	argv[0] = 0;

	/* Set the update new_sha1 and, if specified, old_sha1: */
	if (!argv[1])
		die("missing new value on line: %s", line);
	if (*argv[1] && get_sha1(argv[1], update->new_sha1))
		die("invalid new value on line: %s", line);
	if (argv[2]) {
		update->have_old = 1;
		if (*argv[2] && get_sha1(argv[2], update->old_sha1))
			die("invalid old value on line: %s", line);
	}

	while (argc > 0)
		free(argv[--argc]);
}

int cmd_update_ref(int argc, const char **argv, const char *prefix)
{
	const char *refname, *oldval, *msg = NULL;
	unsigned char sha1[20], oldsha1[20];
	int delete = 0, no_deref = 0, read_stdin = 0, flags = 0;
	struct strbuf line = STRBUF_INIT;
	struct option options[] = {
		OPT_STRING( 'm', NULL, &msg, N_("reason"), N_("reason of the update")),
		OPT_BOOL('d', NULL, &delete, N_("delete the reference")),
		OPT_BOOL( 0 , "no-deref", &no_deref,
					N_("update <refname> not the one it points to")),
		OPT_BOOLEAN( 0 , "stdin", &read_stdin, N_("read updates from stdin")),
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
		while (strbuf_getline(&line, stdin, '\n') != EOF)
			update_refs_stdin(line.buf);
		strbuf_release(&line);
		return update_refs(msg, updates, updates_count, DIE_ON_ERR);
	}

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
