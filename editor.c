#include "cache.h"
#include "strbuf.h"
#include "run-command.h"
#include "sigchain.h"

#ifndef DEFAULT_EDITOR
#define DEFAULT_EDITOR "vi"
#endif

const char *git_editor(void)
{
	const char *editor = getenv("GIT_EDITOR");
	const char *terminal = getenv("TERM");
	int terminal_is_dumb = !terminal || !strcmp(terminal, "dumb");

	if (!editor && editor_program)
		editor = editor_program;
	if (!editor && !terminal_is_dumb)
		editor = getenv("VISUAL");
	if (!editor)
		editor = getenv("EDITOR");

	if (!editor && terminal_is_dumb)
		return NULL;

	if (!editor)
		editor = DEFAULT_EDITOR;

	return editor;
}

const char *git_sequence_editor(void)
{
	const char *sequence_editor = getenv("GIT_SEQUENCE_EDITOR");

	if (sequence_editor && *sequence_editor)
		return sequence_editor;

	git_config_get_string_const("sequence.editor", &sequence_editor);
	if (sequence_editor && *sequence_editor)
		return sequence_editor;

	return git_editor();
}

static int launch_specific_editor(const char *editor, const char *path, struct strbuf *buffer, const char *const *env)
{
	if (!editor)
		return error("Terminal is dumb, but EDITOR unset");

	if (strcmp(editor, ":")) {
		const char *args[] = { editor, real_path(path), NULL };
		struct child_process p = CHILD_PROCESS_INIT;
		int ret, sig;

		p.argv = args;
		p.env = env;
		p.use_shell = 1;
		if (start_command(&p) < 0)
			return error("unable to start editor '%s'", editor);

		sigchain_push(SIGINT, SIG_IGN);
		sigchain_push(SIGQUIT, SIG_IGN);
		ret = finish_command(&p);
		sig = ret - 128;
		sigchain_pop(SIGINT);
		sigchain_pop(SIGQUIT);
		if (sig == SIGINT || sig == SIGQUIT)
			raise(sig);
		if (ret)
			return error("There was a problem with the editor '%s'.",
					editor);
	}

	if (!buffer)
		return 0;
	if (strbuf_read_file(buffer, path, 0) < 0)
		return error("could not read file '%s': %s",
				path, strerror(errno));

	return 0;
}

int launch_editor(const char *path, struct strbuf *buffer, const char *const *env)
{
	return launch_specific_editor(git_editor(), path, buffer, env);
}

int launch_sequence_editor(const char *path, struct strbuf *buffer, const char *const *env)
{
	return launch_specific_editor(git_sequence_editor(), path, buffer, env);
}
