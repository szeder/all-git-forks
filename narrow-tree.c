#include "cache.h"
#include "narrow-tree.h"
#include "pathspec.h"
#include "argv-array.h"

static struct pathspec narrow_pathspec;

static void validate_prefix(const char *prefix, const char *prev_prefix)
{
	int len = strlen(prefix);

	if (!*prefix)
		die(_("empty line in $GIT_DIR/narrow"));

	if (prefix[len-1] == '/')
		die(_("trailing slash not allowed in $GIT_DIR: %s"), prefix);

	if (!prev_prefix)
		return;

	if (strcmp(prev_prefix, prefix) >= 0)
		die(_("$GIT_DIR/narrow is unsorted at %s"), prefix);
	len = strlen(prev_prefix);
	if (!strncmp(prev_prefix, prefix, len) && prefix[len] == '/')
		die(_("$GIT_DIR/narrow has nested prefix (%s and %s)"),
		    prev_prefix, prefix);
}

void check_narrow_prefix(void)
{
	struct strbuf sb = STRBUF_INIT;
	struct argv_array av = ARGV_ARRAY_INIT;
	char *p, *end;
	static int initialized;

	if (initialized)
		return;

	if (strbuf_read_file(&sb, git_path("narrow"), 0) < 0) {
		if (errno != ENOENT)
			die_errno(_("failed to read %s"), git_path("narrow"));
		return;
	}

	p = sb.buf;
	end = p + sb.len;
	while (p < end) {
		char *nl = strchr(p, '\n');

		if (!nl)
			nl = end;
		else
			*nl = '\0';
		validate_prefix(p, av.argc ? av.argv[av.argc - 1] : NULL);
		argv_array_push(&av, xstrdup(p));
		p = nl + 1;
	}
	strbuf_release(&sb);
	parse_pathspec(&narrow_pathspec,
		       PATHSPEC_ALL_MAGIC & ~PATHSPEC_LITERAL,
		       PATHSPEC_LITERAL_PATH,
		       "", av.argv);
	narrow_pathspec.recursive = 1;
	initialized = 1;
}

char *get_narrow_string(void)
{
	struct strbuf sb = STRBUF_INIT;
	const char **prefix;

	check_narrow_prefix();
	prefix = narrow_pathspec._raw;
	if (prefix) {
		while (*prefix) {
			strbuf_addstr(&sb, *prefix);
			strbuf_addch(&sb, '\n');
			prefix++;
		}
	}
	return strbuf_detach(&sb, NULL);
}

struct pathspec *get_narrow_pathspec(void)
{
	check_narrow_prefix();
	return &narrow_pathspec;
}

int is_repository_narrow(void)
{
	struct pathspec *ps = get_narrow_pathspec();
	return ps && ps->nr;
}
