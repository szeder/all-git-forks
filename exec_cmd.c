#include "cache.h"
#include "exec_cmd.h"
#include "quote.h"
#define MAX_ARGS	32

extern char **environ;
static const char *builtin_exec_path = GIT_EXEC_PATH;
static const char *argv_exec_path;

void git_set_argv_exec_path(const char *exec_path)
{
	argv_exec_path = exec_path;
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

	return builtin_exec_path;
}

static void add_path(struct strbuf *out, const char *path)
{
	if (path && *path) {
		if (is_absolute_path(path))
			strbuf_addstr(out, path);
		else
			strbuf_addstr(out, make_absolute_path(path));

		strbuf_addch(out, ':');
	}
}

static const char *git_repo_exec_path(void)
{
	static char path_buffer[PATH_MAX + 1];
	static char *path = NULL;
	
	int non_git;
	char *git_dir;
	char cwd[PATH_MAX + 1];
	
	if (!path) {
		
		path = path_buffer;
		path[0] = '\0';
		
		if (!getcwd(cwd, PATH_MAX))
			die("git_repo_exec_path: can not getcwd");
		
		setup_git_directory_gently(&non_git);
		
		if (!non_git) {
			
			git_dir = get_git_dir();
			strncat(path, git_dir, PATH_MAX);
			strncat(path, "/", PATH_MAX);
			strncat(path, "bin", PATH_MAX);
			
			strncpy(path, make_absolute_path(path), PATH_MAX);
			
			if (chdir(cwd))
				die("git_repo_exec_path: can not chdir to '%s'", cwd);
		}
	}
	
	return path;
}

void setup_path(const char *cmd_path)
{
	const char *old_path = getenv("PATH");
	struct strbuf new_path;

	strbuf_init(&new_path, 0);

	add_path(&new_path, git_repo_exec_path());
	add_path(&new_path, argv_exec_path);
	add_path(&new_path, getenv(EXEC_PATH_ENVIRONMENT));
	add_path(&new_path, builtin_exec_path);
	add_path(&new_path, cmd_path);

	if (old_path)
		strbuf_addstr(&new_path, old_path);
	else
		strbuf_addstr(&new_path, "/usr/local/bin:/usr/bin:/bin");

	setenv("PATH", new_path.buf, 1);

	strbuf_release(&new_path);
}

int execv_git_cmd(const char **argv)
{
	struct strbuf cmd;
	const char *tmp;

	strbuf_init(&cmd, 0);
	strbuf_addf(&cmd, "git-%s", argv[0]);

	/*
	 * argv[0] must be the git command, but the argv array
	 * belongs to the caller, and may be reused in
	 * subsequent loop iterations. Save argv[0] and
	 * restore it on error.
	 */
	tmp = argv[0];
	argv[0] = cmd.buf;

	trace_argv_printf(argv, "trace: exec:");

	/* execvp() can only ever return if it fails */
	execvp(cmd.buf, (char **)argv);

	trace_printf("trace: exec failed: %s\n", strerror(errno));

	argv[0] = tmp;

	strbuf_release(&cmd);

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
