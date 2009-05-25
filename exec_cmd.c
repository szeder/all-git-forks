#include "cache.h"
#include "exec_cmd.h"
#include "quote.h"
#define MAX_ARGS	32

extern char **environ;
static const char *argv_exec_path;
static const char *argv0_path;

const char *system_path(const char *path)
{
	if (!is_absolute_path(path) && argv0_path) {
		struct strbuf d = STRBUF_INIT;
		strbuf_addf(&d, "%s/%s", argv0_path, path);
		path = strbuf_detach(&d, NULL);
	}
	return path;
}

void git_set_argv0_path(const char *path)
{
	argv0_path = path;
}

void git_set_argv_exec_path(const char *exec_path)
{
	argv_exec_path = exec_path;
	/*
	 * Propagate this setting to external programs.
	 */
	setenv(EXEC_PATH_ENVIRONMENT, exec_path, 1);
}


/* Returns the highest-priority, location to look for git programs. */
const char *git_exec_path(void)
{
	const char *env;

	if (argv_exec_path)
		return argv_exec_path;

	env = getenv(EXEC_PATH_ENVIRONMENT);
	if (env && *env) {
		return env;
	}

	return system_path(GIT_EXEC_PATH);
}

/* Returns the path of the bin folder inside the .git folder. */
/* (This could be used to store repository specific git programs.) */

int enable_git_repo_exec_path = 0;

const char *git_repo_exec_path(void)
{
	static char path_buffer[PATH_MAX + 1];
	static char *path = NULL;
	
	int non_git;
	const char *git_dir;
	
	if (!path && enable_git_repo_exec_path) {
		
		path = path_buffer;
		path[0] = '\0';
		
		setup_git_directory_gently(&non_git);
		
		if (!non_git) {
			
			git_dir = get_git_dir();
			
			strncat(path, git_dir, PATH_MAX);
			strncat(path, "/", PATH_MAX);
			strncat(path, "bin", PATH_MAX);
			strncpy(path, make_absolute_path(path), PATH_MAX);
			if (access(path, F_OK) != 0)
				path[0] = '\0';
		}
	}
	
	if (!path || (path[0] == '\0'))
		return NULL;
	
	return path;
}

static void add_path(struct strbuf *out, const char *path)
{
	if (path && *path) {
		if (is_absolute_path(path))
			strbuf_addstr(out, path);
		else
			strbuf_addstr(out, make_nonrelative_path(path));

		strbuf_addch(out, PATH_SEP);
	}
}

void setup_path(void)
{
	const char *old_path = getenv("PATH");
	struct strbuf new_path = STRBUF_INIT;

	if (git_repo_exec_path() != NULL)
		add_path(&new_path, git_repo_exec_path());
	add_path(&new_path, argv_exec_path);
	add_path(&new_path, getenv(EXEC_PATH_ENVIRONMENT));
	add_path(&new_path, system_path(GIT_EXEC_PATH));
	add_path(&new_path, argv0_path);

	if (old_path)
		strbuf_addstr(&new_path, old_path);
	else
		strbuf_addstr(&new_path, "/usr/local/bin:/usr/bin:/bin");

	setenv("PATH", new_path.buf, 1);

	strbuf_release(&new_path);
}

const char **prepare_git_cmd(const char **argv)
{
	int argc;
	const char **nargv;

	for (argc = 0; argv[argc]; argc++)
		; /* just counting */
	nargv = xmalloc(sizeof(*nargv) * (argc + 2));

	nargv[0] = "git";
	for (argc = 0; argv[argc]; argc++)
		nargv[argc + 1] = argv[argc];
	nargv[argc + 1] = NULL;
	return nargv;
}

int execv_git_cmd(const char **argv) {
	const char **nargv = prepare_git_cmd(argv);
	trace_argv_printf(nargv, "trace: exec:");

	/* execvp() can only ever return if it fails */
	execvp("git", (char **)nargv);

	trace_printf("trace: exec failed: %s\n", strerror(errno));

	free(nargv);
	return -1;
}


int execl_git_cmd(const char *cmd,...)
{
	int argc;
	const char *argv[MAX_ARGS + 1];
	const char *arg;
	va_list param;

	va_start(param, cmd);
	argv[0] = cmd;
	argc = 1;
	while (argc < MAX_ARGS) {
		arg = argv[argc++] = va_arg(param, char *);
		if (!arg)
			break;
	}
	va_end(param);
	if (MAX_ARGS <= argc)
		return error("too many args to run %s", cmd);

	argv[argc] = NULL;
	return execv_git_cmd(argv);
}
