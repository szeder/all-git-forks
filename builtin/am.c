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
#include "run-command.h"

enum patch_format {
	PATCH_FORMAT_UNKNOWN = 0,
	PATCH_FORMAT_MBOX
};

struct am_state {
	/* state directory path */
	struct strbuf dir;

	/* current and last patch numbers, 1-indexed */
	int cur;
	int last;

	/* number of digits in patch filename */
	int prec;
};

/**
 * Initializes am_state with the default values.
 */
static void am_state_init(struct am_state *state)
{
	memset(state, 0, sizeof(*state));

	strbuf_init(&state->dir, 0);
	state->prec = 4;
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

/*
 * Returns 1 if the file looks like a piece of email a-la RFC2822, 0 otherwise.
 * We check this by grabbing all the non-indented lines and seeing if they look
 * like they begin with valid header field names.
 */
static int is_email(const char *filename)
{
	struct strbuf sb = STRBUF_INIT;
	FILE *fp = xfopen(filename, "r");
	int ret = 1;

	while (!strbuf_getline(&sb, fp, '\n')) {
		const char *x;

		strbuf_rtrim(&sb);

		if (!sb.len)
			break; /* End of header */

		/* Ignore indented folded lines */
		if (*sb.buf == '\t' || *sb.buf == ' ')
			continue;

		/* It's a header if it matches the regexp "^[!-9;-~]+:" */
		for (x = sb.buf; *x; x++) {
			if (('!' <= *x && *x <= '9') || (';' <= *x && *x <= '~'))
				continue;
			if (*x == ':' && x != sb.buf)
				break;
			ret = 0;
			goto done;
		}
	}

done:
	fclose(fp);
	strbuf_release(&sb);
	return ret;
}

/**
 * Attempts to detect the patch_format of the patches contained in `paths`,
 * returning the PATCH_FORMAT_* enum value. Returns PATCH_FORMAT_UNKNOWN if
 * detection fails.
 */
static int detect_patch_format(struct string_list *paths)
{
	enum patch_format ret = PATCH_FORMAT_UNKNOWN;
	struct strbuf l1 = STRBUF_INIT;
	struct strbuf l2 = STRBUF_INIT;
	struct strbuf l3 = STRBUF_INIT;
	FILE *fp;

	/*
	 * We default to mbox format if input is from stdin and for directories
	 */
	if (!paths->nr || !strcmp(paths->items->string, "-") ||
	    is_directory(paths->items->string)) {
		ret = PATCH_FORMAT_MBOX;
		goto done;
	}

	/*
	 * Otherwise, check the first 3 lines of the first patch, starting
	 * from the first non-blank line, to try to detect its format.
	 */
	fp = xfopen(paths->items->string, "r");
	while (!strbuf_getline(&l1, fp, '\n')) {
		strbuf_trim(&l1);
		if (l1.len)
			break;
	}
	strbuf_getline(&l2, fp, '\n');
	strbuf_trim(&l2);
	strbuf_getline(&l3, fp, '\n');
	strbuf_trim(&l3);
	fclose(fp);

	if (starts_with(l1.buf, "From ") || starts_with(l1.buf, "From: "))
		ret = PATCH_FORMAT_MBOX;
	else if (l1.len && l2.len && l3.len && is_email(paths->items->string))
		ret = PATCH_FORMAT_MBOX;

done:
	strbuf_release(&l1);
	strbuf_release(&l2);
	strbuf_release(&l3);
	return ret;
}

/**
 * Splits out individual patches from `paths`, where each path is either a mbox
 * file or a Maildir. Return 0 on success, -1 on failure.
 */
static int split_patches_mbox(struct am_state *state, struct string_list *paths)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	struct string_list_item *item;
	struct strbuf last = STRBUF_INIT;

	cp.git_cmd = 1;
	argv_array_push(&cp.args, "mailsplit");
	argv_array_pushf(&cp.args, "-d%d", state->prec);
	argv_array_pushf(&cp.args, "-o%s", state->dir.buf);
	argv_array_push(&cp.args, "-b");
	argv_array_push(&cp.args, "--");

	for_each_string_list_item(item, paths)
		argv_array_push(&cp.args, item->string);

	if (capture_command(&cp, &last, 8))
		return -1;

	state->cur = 1;
	state->last = strtol(last.buf, NULL, 10);

	return 0;
}

/**
 * Splits out individual patches, of patch_format, contained within paths.
 * These patches will be stored in the state directory, with each patch's
 * filename being its index, padded to state->prec digits. state->cur will be
 * set to the index of the first patch, and state->last will be set to the
 * index of the last patch. Returns 0 on success, -1 on failure.
 */
static int split_patches(struct am_state *state, enum patch_format patch_format,
		struct string_list *paths)
{
	switch (patch_format) {
	case PATCH_FORMAT_MBOX:
		return split_patches_mbox(state, paths);
	default:
		die("BUG: invalid patch_format");
	}
	return -1;
}

/**
 * Setup a new am session for applying patches
 */
static void am_setup(struct am_state *state, enum patch_format patch_format,
		struct string_list *paths)
{
	if (!patch_format)
		patch_format = detect_patch_format(paths);

	if (!patch_format) {
		fprintf_ln(stderr, _("Patch format detection failed."));
		exit(128);
	}

	if (mkdir(state->dir.buf, 0777) < 0 && errno != EEXIST)
		die_errno(_("failed to create directory '%s'"), state->dir.buf);

	if (split_patches(state, patch_format, paths) < 0) {
		am_destroy(state);
		die(_("Failed to split patches."));
	}

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
	while (state->cur <= state->last) {

		/* TODO: Patch application not implemented yet */

		am_next(state);
	}

	am_destroy(state);
}

/**
 * parse_options() callback that validates and sets opt->value to the
 * PATCH_FORMAT_* enum value corresponding to `arg`.
 */
static int parse_opt_patchformat(const struct option *opt, const char *arg, int unset)
{
	int *opt_value = opt->value;

	if (!strcmp(arg, "mbox"))
		*opt_value = PATCH_FORMAT_MBOX;
	else
		return -1;
	return 0;
}

static struct am_state state;
static int opt_patch_format;

static const char * const am_usage[] = {
	N_("git am [options] [(<mbox>|<Maildir>)...]"),
	NULL
};

static struct option am_options[] = {
	OPT_CALLBACK(0, "patch-format", &opt_patch_format, N_("format"),
		N_("format the patch(es) are in"), parse_opt_patchformat),
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
	else {
		struct string_list paths = STRING_LIST_INIT_DUP;
		int i;

		for (i = 0; i < argc; i++) {
			if (is_absolute_path(argv[i]) || !prefix)
				string_list_append(&paths, argv[i]);
			else
				string_list_append(&paths, mkpath("%s/%s", prefix, argv[i]));
		}

		am_setup(&state, opt_patch_format, &paths);

		string_list_clear(&paths, 0);
	}

	am_run(&state);

	am_state_release(&state);

	return 0;
}
