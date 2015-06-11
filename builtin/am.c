/*
 * Builtin "git am"
 *
 * Based on git-am.sh by Junio C Hamano.
 */
#include "cache.h"
#include "builtin.h"
#include "exec_cmd.h"
#include "parse-options.h"
#include "dir.h"

struct am_state {
	/* state directory path */
	struct strbuf dir;

	/* current and last patch numbers, 1-indexed */
	int cur;
	int last;
};

/**
 * Initializes am_state with the default values.
 */
static void am_state_init(struct am_state *state)
{
	memset(state, 0, sizeof(*state));

	strbuf_init(&state->dir, 0);
}

/**
 * Release memory allocated by an am_state.
 */
static void am_state_release(struct am_state *state)
{
	strbuf_release(&state->dir);
}

/**
 * Returns path relative to the am_state directory.
 */
static inline const char *am_path(const struct am_state *state, const char *path)
{
	return mkpath("%s/%s", state->dir.buf, path);
}

/**
 * Returns 1 if there is an am session in progress, 0 otherwise.
 */
static int am_in_progress(const struct am_state *state)
{
	struct stat st;

	if (lstat(state->dir.buf, &st) < 0 || !S_ISDIR(st.st_mode))
		return 0;
	if (lstat(am_path(state, "last"), &st) || !S_ISREG(st.st_mode))
		return 0;
	if (lstat(am_path(state, "next"), &st) || !S_ISREG(st.st_mode))
		return 0;
	return 1;
}

/**
 * Reads the contents of `file`. The third argument can be used to give a hint
 * about the file size, to avoid reallocs. Returns number of bytes read on
 * success, -1 if the file does not exist. If trim is set, trailing whitespace
 * will be removed from the file contents.
 */
static int read_state_file(struct strbuf *sb, const char *file, size_t hint, int trim)
{
	strbuf_reset(sb);
	if (strbuf_read_file(sb, file, hint) >= 0) {
		if (trim)
			strbuf_rtrim(sb);

		return sb->len;
	}

	if (errno == ENOENT)
		return -1;

	die_errno(_("could not read '%s'"), file);
}

/**
 * Loads state from disk.
 */
static void am_load(struct am_state *state)
{
	struct strbuf sb = STRBUF_INIT;

	read_state_file(&sb, am_path(state, "next"), 8, 1);
	state->cur = strtol(sb.buf, NULL, 10);

	read_state_file(&sb, am_path(state, "last"), 8, 1);
	state->last = strtol(sb.buf, NULL, 10);

	strbuf_release(&sb);
}

/**
 * Remove the am_state directory.
 */
static void am_destroy(const struct am_state *state)
{
	struct strbuf sb = STRBUF_INIT;

	strbuf_addstr(&sb, state->dir.buf);
	remove_dir_recursively(&sb, 0);
	strbuf_release(&sb);
}

/**
 * Setup a new am session for applying patches
 */
static void am_setup(struct am_state *state)
{
	if (mkdir(state->dir.buf, 0777) < 0 && errno != EEXIST)
		die_errno(_("failed to create directory '%s'"), state->dir.buf);

	write_file(am_path(state, "next"), 1, "%d", state->cur);

	write_file(am_path(state, "last"), 1, "%d", state->last);
}

/**
 * Increments the patch pointer, and cleans am_state for the application of the
 * next patch.
 */
static void am_next(struct am_state *state)
{
	state->cur++;
	write_file(am_path(state, "next"), 1, "%d", state->cur);
}

/**
 * Applies all queued patches.
 */
static void am_run(struct am_state *state)
{
	while (state->cur <= state->last)
		am_next(state);

	am_destroy(state);
}

static struct am_state state;

static const char * const am_usage[] = {
	N_("git am [options] [(<mbox>|<Maildir>)...]"),
	NULL
};

static struct option am_options[] = {
	OPT_END()
};

int cmd_am(int argc, const char **argv, const char *prefix)
{
	if (!getenv("_GIT_USE_BUILTIN_AM")) {
		const char *path = mkpath("%s/git-am", git_exec_path());

		if (sane_execvp(path, (char**) argv) < 0)
			die_errno("could not exec %s", path);
	}

	git_config(git_default_config, NULL);

	am_state_init(&state);
	strbuf_addstr(&state.dir, git_path("rebase-apply"));

	argc = parse_options(argc, argv, prefix, am_options, am_usage, 0);

	if (am_in_progress(&state))
		am_load(&state);
	else
		am_setup(&state);

	am_run(&state);

	am_state_release(&state);

	return 0;
}
