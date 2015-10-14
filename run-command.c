#include "cache.h"
#include "run-command.h"
#include "exec_cmd.h"
#include "sigchain.h"
#include "argv-array.h"
#include "thread-utils.h"
#include "strbuf.h"

void child_process_init(struct child_process *child)
{
	memset(child, 0, sizeof(*child));
	argv_array_init(&child->args);
	argv_array_init(&child->env_array);
}

struct child_to_clean {
	pid_t pid;
	struct child_to_clean *next;
};
static struct child_to_clean *children_to_clean;
static int installed_child_cleanup_handler;

static void cleanup_children(int sig, int in_signal)
{
	while (children_to_clean) {
		struct child_to_clean *p = children_to_clean;
		children_to_clean = p->next;
		kill(p->pid, sig);
		if (!in_signal)
			free(p);
	}
}

static void cleanup_children_on_signal(int sig)
{
	cleanup_children(sig, 1);
	sigchain_pop(sig);
	raise(sig);
}

static void cleanup_children_on_exit(void)
{
	cleanup_children(SIGTERM, 0);
}

static void mark_child_for_cleanup(pid_t pid)
{
	struct child_to_clean *p = xmalloc(sizeof(*p));
	p->pid = pid;
	p->next = children_to_clean;
	children_to_clean = p;

	if (!installed_child_cleanup_handler) {
		atexit(cleanup_children_on_exit);
		sigchain_push_common(cleanup_children_on_signal);
		installed_child_cleanup_handler = 1;
	}
}

static void clear_child_for_cleanup(pid_t pid)
{
	struct child_to_clean **pp;

	for (pp = &children_to_clean; *pp; pp = &(*pp)->next) {
		struct child_to_clean *clean_me = *pp;

		if (clean_me->pid == pid) {
			*pp = clean_me->next;
			free(clean_me);
			return;
		}
	}
}

static inline void close_pair(int fd[2])
{
	close(fd[0]);
	close(fd[1]);
}

#ifndef GIT_WINDOWS_NATIVE
static inline void dup_devnull(int to)
{
	int fd = open("/dev/null", O_RDWR);
	if (fd < 0)
		die_errno(_("open /dev/null failed"));
	if (dup2(fd, to) < 0)
		die_errno(_("dup2(%d,%d) failed"), fd, to);
	close(fd);
}
#endif

static char *locate_in_PATH(const char *file)
{
	const char *p = getenv("PATH");
	struct strbuf buf = STRBUF_INIT;

	if (!p || !*p)
		return NULL;

	while (1) {
		const char *end = strchrnul(p, ':');

		strbuf_reset(&buf);

		/* POSIX specifies an empty entry as the current directory. */
		if (end != p) {
			strbuf_add(&buf, p, end - p);
			strbuf_addch(&buf, '/');
		}
		strbuf_addstr(&buf, file);

		if (!access(buf.buf, F_OK))
			return strbuf_detach(&buf, NULL);

		if (!*end)
			break;
		p = end + 1;
	}

	strbuf_release(&buf);
	return NULL;
}

static int exists_in_PATH(const char *file)
{
	char *r = locate_in_PATH(file);
	free(r);
	return r != NULL;
}

int sane_execvp(const char *file, char * const argv[])
{
	if (!execvp(file, argv))
		return 0; /* cannot happen ;-) */

	/*
	 * When a command can't be found because one of the directories
	 * listed in $PATH is unsearchable, execvp reports EACCES, but
	 * careful usability testing (read: analysis of occasional bug
	 * reports) reveals that "No such file or directory" is more
	 * intuitive.
	 *
	 * We avoid commands with "/", because execvp will not do $PATH
	 * lookups in that case.
	 *
	 * The reassignment of EACCES to errno looks like a no-op below,
	 * but we need to protect against exists_in_PATH overwriting errno.
	 */
	if (errno == EACCES && !strchr(file, '/'))
		errno = exists_in_PATH(file) ? EACCES : ENOENT;
	else if (errno == ENOTDIR && !strchr(file, '/'))
		errno = ENOENT;
	return -1;
}

static const char **prepare_shell_cmd(const char **argv)
{
	int argc, nargc = 0;
	const char **nargv;

	for (argc = 0; argv[argc]; argc++)
		; /* just counting */
	/* +1 for NULL, +3 for "sh -c" plus extra $0 */
	nargv = xmalloc(sizeof(*nargv) * (argc + 1 + 3));

	if (argc < 1)
		die("BUG: shell command is empty");

	if (strcspn(argv[0], "|&;<>()$`\\\"' \t\n*?[#~=%") != strlen(argv[0])) {
#ifndef GIT_WINDOWS_NATIVE
		nargv[nargc++] = SHELL_PATH;
#else
		nargv[nargc++] = "sh";
#endif
		nargv[nargc++] = "-c";

		if (argc < 2)
			nargv[nargc++] = argv[0];
		else {
			struct strbuf arg0 = STRBUF_INIT;
			strbuf_addf(&arg0, "%s \"$@\"", argv[0]);
			nargv[nargc++] = strbuf_detach(&arg0, NULL);
		}
	}

	for (argc = 0; argv[argc]; argc++)
		nargv[nargc++] = argv[argc];
	nargv[nargc] = NULL;

	return nargv;
}

#ifndef GIT_WINDOWS_NATIVE
static int execv_shell_cmd(const char **argv)
{
	const char **nargv = prepare_shell_cmd(argv);
	trace_argv_printf(nargv, "trace: exec:");
	sane_execvp(nargv[0], (char **)nargv);
	free(nargv);
	return -1;
}
#endif

#ifndef GIT_WINDOWS_NATIVE
static int child_notifier = -1;

static void notify_parent(void)
{
	/*
	 * execvp failed.  If possible, we'd like to let start_command
	 * know, so failures like ENOENT can be handled right away; but
	 * otherwise, finish_command will still report the error.
	 */
	xwrite(child_notifier, "", 1);
}
#endif

static inline void set_cloexec(int fd)
{
	int flags = fcntl(fd, F_GETFD);
	if (flags >= 0)
		fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static int wait_or_whine(pid_t pid, const char *argv0, int in_signal)
{
	int status, code = -1;
	pid_t waiting;
	int failed_errno = 0;

	while ((waiting = waitpid(pid, &status, 0)) < 0 && errno == EINTR)
		;	/* nothing */
	if (in_signal)
		return 0;

	if (waiting < 0) {
		failed_errno = errno;
		error("waitpid for %s failed: %s", argv0, strerror(errno));
	} else if (waiting != pid) {
		error("waitpid is confused (%s)", argv0);
	} else if (WIFSIGNALED(status)) {
		code = WTERMSIG(status);
		if (code != SIGINT && code != SIGQUIT)
			error("%s died of signal %d", argv0, code);
		/*
		 * This return value is chosen so that code & 0xff
		 * mimics the exit code that a POSIX shell would report for
		 * a program that died from this signal.
		 */
		code += 128;
	} else if (WIFEXITED(status)) {
		code = WEXITSTATUS(status);
		/*
		 * Convert special exit code when execvp failed.
		 */
		if (code == 127) {
			code = -1;
			failed_errno = ENOENT;
		}
	} else {
		error("waitpid is confused (%s)", argv0);
	}

	clear_child_for_cleanup(pid);

	errno = failed_errno;
	return code;
}

int start_command(struct child_process *cmd)
{
	int need_in, need_out, need_err;
	int fdin[2], fdout[2], fderr[2];
	int failed_errno;
	char *str;

	if (!cmd->argv)
		cmd->argv = cmd->args.argv;
	if (!cmd->env)
		cmd->env = cmd->env_array.argv;

	/*
	 * In case of errors we must keep the promise to close FDs
	 * that have been passed in via ->in and ->out.
	 */

	need_in = !cmd->no_stdin && cmd->in < 0;
	if (need_in) {
		if (pipe(fdin) < 0) {
			failed_errno = errno;
			if (cmd->out > 0)
				close(cmd->out);
			str = "standard input";
			goto fail_pipe;
		}
		cmd->in = fdin[1];
	}

	need_out = !cmd->no_stdout
		&& !cmd->stdout_to_stderr
		&& cmd->out < 0;
	if (need_out) {
		if (pipe(fdout) < 0) {
			failed_errno = errno;
			if (need_in)
				close_pair(fdin);
			else if (cmd->in)
				close(cmd->in);
			str = "standard output";
			goto fail_pipe;
		}
		cmd->out = fdout[0];
	}

	need_err = !cmd->no_stderr && cmd->err < 0;
	if (need_err) {
		if (pipe(fderr) < 0) {
			failed_errno = errno;
			if (need_in)
				close_pair(fdin);
			else if (cmd->in)
				close(cmd->in);
			if (need_out)
				close_pair(fdout);
			else if (cmd->out)
				close(cmd->out);
			str = "standard error";
fail_pipe:
			error("cannot create %s pipe for %s: %s",
				str, cmd->argv[0], strerror(failed_errno));
			argv_array_clear(&cmd->args);
			argv_array_clear(&cmd->env_array);
			errno = failed_errno;
			return -1;
		}
		cmd->err = fderr[0];
	}

	trace_argv_printf(cmd->argv, "trace: run_command:");
	fflush(NULL);

#ifndef GIT_WINDOWS_NATIVE
{
	int notify_pipe[2];
	if (pipe(notify_pipe))
		notify_pipe[0] = notify_pipe[1] = -1;

	cmd->pid = fork();
	failed_errno = errno;
	if (!cmd->pid) {
		/*
		 * Redirect the channel to write syscall error messages to
		 * before redirecting the process's stderr so that all die()
		 * in subsequent call paths use the parent's stderr.
		 */
		if (cmd->no_stderr || need_err) {
			int child_err = dup(2);
			set_cloexec(child_err);
			set_error_handle(fdopen(child_err, "w"));
		}

		close(notify_pipe[0]);
		set_cloexec(notify_pipe[1]);
		child_notifier = notify_pipe[1];
		atexit(notify_parent);

		if (cmd->no_stdin)
			dup_devnull(0);
		else if (need_in) {
			dup2(fdin[0], 0);
			close_pair(fdin);
		} else if (cmd->in) {
			dup2(cmd->in, 0);
			close(cmd->in);
		}

		if (cmd->no_stderr)
			dup_devnull(2);
		else if (need_err) {
			dup2(fderr[1], 2);
			close_pair(fderr);
		} else if (cmd->err > 1) {
			dup2(cmd->err, 2);
			close(cmd->err);
		}

		if (cmd->no_stdout)
			dup_devnull(1);
		else if (cmd->stdout_to_stderr)
			dup2(2, 1);
		else if (need_out) {
			dup2(fdout[1], 1);
			close_pair(fdout);
		} else if (cmd->out > 1) {
			dup2(cmd->out, 1);
			close(cmd->out);
		}

		if (cmd->dir && chdir(cmd->dir))
			die_errno("exec '%s': cd to '%s' failed", cmd->argv[0],
			    cmd->dir);
		if (cmd->env) {
			for (; *cmd->env; cmd->env++) {
				if (strchr(*cmd->env, '='))
					putenv((char *)*cmd->env);
				else
					unsetenv(*cmd->env);
			}
		}
		if (cmd->git_cmd)
			execv_git_cmd(cmd->argv);
		else if (cmd->use_shell)
			execv_shell_cmd(cmd->argv);
		else
			sane_execvp(cmd->argv[0], (char *const*) cmd->argv);
		if (errno == ENOENT) {
			if (!cmd->silent_exec_failure)
				error("cannot run %s: %s", cmd->argv[0],
					strerror(ENOENT));
			exit(127);
		} else {
			die_errno("cannot exec '%s'", cmd->argv[0]);
		}
	}
	if (cmd->pid < 0)
		error("cannot fork() for %s: %s", cmd->argv[0],
			strerror(errno));
	else if (cmd->clean_on_exit)
		mark_child_for_cleanup(cmd->pid);

	/*
	 * Wait for child's execvp. If the execvp succeeds (or if fork()
	 * failed), EOF is seen immediately by the parent. Otherwise, the
	 * child process sends a single byte.
	 * Note that use of this infrastructure is completely advisory,
	 * therefore, we keep error checks minimal.
	 */
	close(notify_pipe[1]);
	if (read(notify_pipe[0], &notify_pipe[1], 1) == 1) {
		/*
		 * At this point we know that fork() succeeded, but execvp()
		 * failed. Errors have been reported to our stderr.
		 */
		wait_or_whine(cmd->pid, cmd->argv[0], 0);
		failed_errno = errno;
		cmd->pid = -1;
	}
	close(notify_pipe[0]);
}
#else
{
	int fhin = 0, fhout = 1, fherr = 2;
	const char **sargv = cmd->argv;

	if (cmd->no_stdin)
		fhin = open("/dev/null", O_RDWR);
	else if (need_in)
		fhin = dup(fdin[0]);
	else if (cmd->in)
		fhin = dup(cmd->in);

	if (cmd->no_stderr)
		fherr = open("/dev/null", O_RDWR);
	else if (need_err)
		fherr = dup(fderr[1]);
	else if (cmd->err > 2)
		fherr = dup(cmd->err);

	if (cmd->no_stdout)
		fhout = open("/dev/null", O_RDWR);
	else if (cmd->stdout_to_stderr)
		fhout = dup(fherr);
	else if (need_out)
		fhout = dup(fdout[1]);
	else if (cmd->out > 1)
		fhout = dup(cmd->out);

	if (cmd->git_cmd)
		cmd->argv = prepare_git_cmd(cmd->argv);
	else if (cmd->use_shell)
		cmd->argv = prepare_shell_cmd(cmd->argv);

	cmd->pid = mingw_spawnvpe(cmd->argv[0], cmd->argv, (char**) cmd->env,
			cmd->dir, fhin, fhout, fherr);
	failed_errno = errno;
	if (cmd->pid < 0 && (!cmd->silent_exec_failure || errno != ENOENT))
		error("cannot spawn %s: %s", cmd->argv[0], strerror(errno));
	if (cmd->clean_on_exit && cmd->pid >= 0)
		mark_child_for_cleanup(cmd->pid);

	if (cmd->git_cmd)
		free(cmd->argv);

	cmd->argv = sargv;
	if (fhin != 0)
		close(fhin);
	if (fhout != 1)
		close(fhout);
	if (fherr != 2)
		close(fherr);
}
#endif

	if (cmd->pid < 0) {
		if (need_in)
			close_pair(fdin);
		else if (cmd->in)
			close(cmd->in);
		if (need_out)
			close_pair(fdout);
		else if (cmd->out)
			close(cmd->out);
		if (need_err)
			close_pair(fderr);
		else if (cmd->err)
			close(cmd->err);
		argv_array_clear(&cmd->args);
		argv_array_clear(&cmd->env_array);
		errno = failed_errno;
		return -1;
	}

	if (need_in)
		close(fdin[0]);
	else if (cmd->in)
		close(cmd->in);

	if (need_out)
		close(fdout[1]);
	else if (cmd->out)
		close(cmd->out);

	if (need_err)
		close(fderr[1]);
	else if (cmd->err)
		close(cmd->err);

	return 0;
}

int finish_command(struct child_process *cmd)
{
	int ret = wait_or_whine(cmd->pid, cmd->argv[0], 0);
	argv_array_clear(&cmd->args);
	argv_array_clear(&cmd->env_array);
	return ret;
}

int finish_command_in_signal(struct child_process *cmd)
{
	return wait_or_whine(cmd->pid, cmd->argv[0], 1);
}


int run_command(struct child_process *cmd)
{
	int code;

	if (cmd->out < 0 || cmd->err < 0)
		die("BUG: run_command with a pipe can cause deadlock");

	code = start_command(cmd);
	if (code)
		return code;
	return finish_command(cmd);
}

int run_command_v_opt(const char **argv, int opt)
{
	return run_command_v_opt_cd_env(argv, opt, NULL, NULL);
}

int run_command_v_opt_cd_env(const char **argv, int opt, const char *dir, const char *const *env)
{
	struct child_process cmd = CHILD_PROCESS_INIT;
	cmd.argv = argv;
	cmd.no_stdin = opt & RUN_COMMAND_NO_STDIN ? 1 : 0;
	cmd.git_cmd = opt & RUN_GIT_CMD ? 1 : 0;
	cmd.stdout_to_stderr = opt & RUN_COMMAND_STDOUT_TO_STDERR ? 1 : 0;
	cmd.silent_exec_failure = opt & RUN_SILENT_EXEC_FAILURE ? 1 : 0;
	cmd.use_shell = opt & RUN_USING_SHELL ? 1 : 0;
	cmd.clean_on_exit = opt & RUN_CLEAN_ON_EXIT ? 1 : 0;
	cmd.dir = dir;
	cmd.env = env;
	return run_command(&cmd);
}

#ifndef NO_PTHREADS
static pthread_t main_thread;
static int main_thread_set;
static pthread_key_t async_key;
static pthread_key_t async_die_counter;

static void *run_thread(void *data)
{
	struct async *async = data;
	intptr_t ret;

	pthread_setspecific(async_key, async);
	ret = async->proc(async->proc_in, async->proc_out, async->data);
	return (void *)ret;
}

static NORETURN void die_async(const char *err, va_list params)
{
	vreportf("fatal: ", err, params);

	if (in_async()) {
		struct async *async = pthread_getspecific(async_key);
		if (async->proc_in >= 0)
			close(async->proc_in);
		if (async->proc_out >= 0)
			close(async->proc_out);
		pthread_exit((void *)128);
	}

	exit(128);
}

static int async_die_is_recursing(void)
{
	void *ret = pthread_getspecific(async_die_counter);
	pthread_setspecific(async_die_counter, (void *)1);
	return ret != NULL;
}

int in_async(void)
{
	if (!main_thread_set)
		return 0; /* no asyncs started yet */
	return !pthread_equal(main_thread, pthread_self());
}

#else

static struct {
	void (**handlers)(void);
	size_t nr;
	size_t alloc;
} git_atexit_hdlrs;

static int git_atexit_installed;

static void git_atexit_dispatch(void)
{
	size_t i;

	for (i=git_atexit_hdlrs.nr ; i ; i--)
		git_atexit_hdlrs.handlers[i-1]();
}

static void git_atexit_clear(void)
{
	free(git_atexit_hdlrs.handlers);
	memset(&git_atexit_hdlrs, 0, sizeof(git_atexit_hdlrs));
	git_atexit_installed = 0;
}

#undef atexit
int git_atexit(void (*handler)(void))
{
	ALLOC_GROW(git_atexit_hdlrs.handlers, git_atexit_hdlrs.nr + 1, git_atexit_hdlrs.alloc);
	git_atexit_hdlrs.handlers[git_atexit_hdlrs.nr++] = handler;
	if (!git_atexit_installed) {
		if (atexit(&git_atexit_dispatch))
			return -1;
		git_atexit_installed = 1;
	}
	return 0;
}
#define atexit git_atexit

static int process_is_async;
int in_async(void)
{
	return process_is_async;
}

#endif

int start_async(struct async *async)
{
	int need_in, need_out;
	int fdin[2], fdout[2];
	int proc_in, proc_out;

	need_in = async->in < 0;
	if (need_in) {
		if (pipe(fdin) < 0) {
			if (async->out > 0)
				close(async->out);
			return error("cannot create pipe: %s", strerror(errno));
		}
		async->in = fdin[1];
	}

	need_out = async->out < 0;
	if (need_out) {
		if (pipe(fdout) < 0) {
			if (need_in)
				close_pair(fdin);
			else if (async->in)
				close(async->in);
			return error("cannot create pipe: %s", strerror(errno));
		}
		async->out = fdout[0];
	}

	if (need_in)
		proc_in = fdin[0];
	else if (async->in)
		proc_in = async->in;
	else
		proc_in = -1;

	if (need_out)
		proc_out = fdout[1];
	else if (async->out)
		proc_out = async->out;
	else
		proc_out = -1;

#ifdef NO_PTHREADS
	/* Flush stdio before fork() to avoid cloning buffers */
	fflush(NULL);

	async->pid = fork();
	if (async->pid < 0) {
		error("fork (async) failed: %s", strerror(errno));
		goto error;
	}
	if (!async->pid) {
		if (need_in)
			close(fdin[1]);
		if (need_out)
			close(fdout[0]);
		git_atexit_clear();
		process_is_async = 1;
		exit(!!async->proc(proc_in, proc_out, async->data));
	}

	mark_child_for_cleanup(async->pid);

	if (need_in)
		close(fdin[0]);
	else if (async->in)
		close(async->in);

	if (need_out)
		close(fdout[1]);
	else if (async->out)
		close(async->out);
#else
	if (!main_thread_set) {
		/*
		 * We assume that the first time that start_async is called
		 * it is from the main thread.
		 */
		main_thread_set = 1;
		main_thread = pthread_self();
		pthread_key_create(&async_key, NULL);
		pthread_key_create(&async_die_counter, NULL);
		set_die_routine(die_async);
		set_die_is_recursing_routine(async_die_is_recursing);
	}

	if (proc_in >= 0)
		set_cloexec(proc_in);
	if (proc_out >= 0)
		set_cloexec(proc_out);
	async->proc_in = proc_in;
	async->proc_out = proc_out;
	{
		int err = pthread_create(&async->tid, NULL, run_thread, async);
		if (err) {
			error("cannot create thread: %s", strerror(err));
			goto error;
		}
	}
#endif
	return 0;

error:
	if (need_in)
		close_pair(fdin);
	else if (async->in)
		close(async->in);

	if (need_out)
		close_pair(fdout);
	else if (async->out)
		close(async->out);
	return -1;
}

int finish_async(struct async *async)
{
#ifdef NO_PTHREADS
	return wait_or_whine(async->pid, "child process", 0);
#else
	void *ret = (void *)(intptr_t)(-1);

	if (pthread_join(async->tid, &ret))
		error("pthread_join failed");
	return (int)(intptr_t)ret;
#endif
}

const char *find_hook(const char *name)
{
	static struct strbuf path = STRBUF_INIT;

	strbuf_reset(&path);
	strbuf_git_path(&path, "hooks/%s", name);
	if (access(path.buf, X_OK) < 0)
		return NULL;
	return path.buf;
}

int run_hook_ve(const char *const *env, const char *name, va_list args)
{
	struct child_process hook = CHILD_PROCESS_INIT;
	const char *p;

	p = find_hook(name);
	if (!p)
		return 0;

	argv_array_push(&hook.args, p);
	while ((p = va_arg(args, const char *)))
		argv_array_push(&hook.args, p);
	hook.env = env;
	hook.no_stdin = 1;
	hook.stdout_to_stderr = 1;

	return run_command(&hook);
}

int run_hook_le(const char *const *env, const char *name, ...)
{
	va_list args;
	int ret;

	va_start(args, name);
	ret = run_hook_ve(env, name, args);
	va_end(args);

	return ret;
}

int capture_command(struct child_process *cmd, struct strbuf *buf, size_t hint)
{
	cmd->out = -1;
	if (start_command(cmd) < 0)
		return -1;

	if (strbuf_read(buf, cmd->out, hint) < 0) {
		close(cmd->out);
		finish_command(cmd); /* throw away exit code */
		return -1;
	}

	close(cmd->out);
	return finish_command(cmd);
}

static struct parallel_processes {
	void *data;

	int max_processes;
	int nr_processes;

	get_next_task_fn get_next_task;
	start_failure_fn start_failure;
	task_finished_fn task_finished;

	struct {
		unsigned in_use : 1;
		struct child_process process;
		struct strbuf err;
		void *data;
	} *children;
	/*
	 * The struct pollfd is logically part of *children,
	 * but the system call expects it as its own array.
	 */
	struct pollfd *pfd;

	unsigned shutdown : 1;

	int output_owner;
	struct strbuf buffered_output; /* of finished children */
} parallel_processes_struct;

static int default_start_failure(struct child_process *cp,
				 struct strbuf *err,
				 void *pp_cb,
				 void *pp_task_cb)
{
	int i;

	strbuf_addstr(err, "Starting a child failed:");
	for (i = 0; cp->argv[i]; i++)
		strbuf_addf(err, " %s", cp->argv[i]);

	return 0;
}

static int default_task_finished(int result,
				 struct child_process *cp,
				 struct strbuf *err,
				 void *pp_cb,
				 void *pp_task_cb)
{
	int i;

	if (!result)
		return 0;

	strbuf_addf(err, "A child failed with return code %d:", result);
	for (i = 0; cp->argv[i]; i++)
		strbuf_addf(err, " %s", cp->argv[i]);

	return 0;
}

static void kill_children(struct parallel_processes *pp, int signo)
{
	int i, n = pp->max_processes;

	for (i = 0; i < n; i++)
		if (pp->children[i].in_use)
			kill(pp->children[i].process.pid, signo);
}

static void handle_children_on_signal(int signo)
{
	struct parallel_processes *pp = &parallel_processes_struct;

	kill_children(pp, signo);
	sigchain_pop(signo);
	raise(signo);
}

static struct parallel_processes *pp_init(int n,
					  get_next_task_fn get_next_task,
					  start_failure_fn start_failure,
					  task_finished_fn task_finished,
					  void *data)
{
	int i;
	struct parallel_processes *pp = &parallel_processes_struct;

	if (n < 1)
		n = online_cpus();

	pp->max_processes = n;
	pp->data = data;
	if (!get_next_task)
		die("BUG: you need to specify a get_next_task function");
	pp->get_next_task = get_next_task;

	pp->start_failure = start_failure ? start_failure : default_start_failure;
	pp->task_finished = task_finished ? task_finished : default_task_finished;

	pp->nr_processes = 0;
	pp->output_owner = 0;
	pp->children = xcalloc(n, sizeof(*pp->children));
	pp->pfd = xcalloc(n, sizeof(*pp->pfd));
	strbuf_init(&pp->buffered_output, 0);

	for (i = 0; i < n; i++) {
		strbuf_init(&pp->children[i].err, 0);
		pp->pfd[i].events = POLLIN;
		pp->pfd[i].fd = -1;
	}
	sigchain_push_common(handle_children_on_signal);
	return pp;
}

static void pp_cleanup(struct parallel_processes *pp)
{
	int i;

	for (i = 0; i < pp->max_processes; i++)
		strbuf_release(&pp->children[i].err);

	free(pp->children);
	free(pp->pfd);
	strbuf_release(&pp->buffered_output);

	sigchain_pop_common();
}

static void set_nonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL);
	if (flags < 0)
		warning("Could not get file status flags, "
			"output will be degraded");
	else if (fcntl(fd, F_SETFL, flags | O_NONBLOCK))
		warning("Could not set file status flags, "
			"output will be degraded");
}

/* returns
 *  0 if a new task was started.
 *  1 if no new jobs was started (get_next_task ran out of work, non critical
 *    problem with starting a new command)
 * -1 no new job was started, user wishes to shutdown early.
 */
static int pp_start_one(struct parallel_processes *pp)
{
	int i;

	for (i = 0; i < pp->max_processes; i++)
		if (!pp->children[i].in_use)
			break;
	if (i == pp->max_processes)
		die("BUG: bookkeeping is hard");

	if (!pp->get_next_task(&pp->children[i].data,
			       &pp->children[i].process,
			       &pp->children[i].err,
			       pp->data))
		return 1;

	if (start_command(&pp->children[i].process)) {
		int code = pp->start_failure(&pp->children[i].process,
					     &pp->children[i].err,
					     pp->data,
					     &pp->children[i].data);
		strbuf_addbuf(&pp->buffered_output, &pp->children[i].err);
		strbuf_reset(&pp->children[i].err);
		return code ? -1 : 1;
	}

	set_nonblocking(pp->children[i].process.err);

	pp->nr_processes++;
	pp->children[i].in_use = 1;
	pp->pfd[i].fd = pp->children[i].process.err;
	return 0;
}

static void pp_buffer_stderr(struct parallel_processes *pp, int output_timeout)
{
	int i;

	while ((i = poll(pp->pfd, pp->max_processes, output_timeout)) < 0) {
		if (errno == EINTR)
			continue;
		pp_cleanup(pp);
		die_errno("poll");
	}

	/* Buffer output from all pipes. */
	for (i = 0; i < pp->max_processes; i++) {
		if (pp->children[i].in_use &&
		    pp->pfd[i].revents & POLLIN)
			if (strbuf_read_once(&pp->children[i].err,
					     pp->children[i].process.err, 0) < 0)
				if (errno != EAGAIN)
					die_errno("read");
	}
}

static void pp_output(struct parallel_processes *pp)
{
	int i = pp->output_owner;
	if (pp->children[i].in_use &&
	    pp->children[i].err.len) {
		fputs(pp->children[i].err.buf, stderr);
		strbuf_reset(&pp->children[i].err);
	}
}

static int pp_collect_finished(struct parallel_processes *pp)
{
	int i = 0;
	pid_t pid;
	int wait_status, code;
	int n = pp->max_processes;
	int result = 0;

	while (pp->nr_processes > 0) {
		pid = waitpid(-1, &wait_status, WNOHANG);
		if (pid == 0)
			return 0;

		if (pid < 0)
			die_errno("wait");

		for (i = 0; i < pp->max_processes; i++)
			if (pp->children[i].in_use &&
			    pid == pp->children[i].process.pid)
				break;
		if (i == pp->max_processes)
			die("BUG: found a child process we were not aware of");

		if (strbuf_read(&pp->children[i].err,
				pp->children[i].process.err, 0) < 0)
			die_errno("strbuf_read");

		if (WIFSIGNALED(wait_status)) {
			code = WTERMSIG(wait_status);
			if (!pp->shutdown &&
			    code != SIGINT && code != SIGQUIT)
				strbuf_addf(&pp->children[i].err,
					    "%s died of signal %d",
					    pp->children[i].process.argv[0],
					    code);
			/*
			 * This return value is chosen so that code & 0xff
			 * mimics the exit code that a POSIX shell would report for
			 * a program that died from this signal.
			 */
			code += 128;
		} else if (WIFEXITED(wait_status)) {
			code = WEXITSTATUS(wait_status);
			/*
			 * Convert special exit code when execvp failed.
			 */
			if (code == 127) {
				code = -1;
				errno = ENOENT;
			}
		} else {
			strbuf_addf(&pp->children[i].err,
				    "waitpid is confused (%s)",
				    pp->children[i].process.argv[0]);
			code = -1;
		}

		if (pp->task_finished(code, &pp->children[i].process,
				      &pp->children[i].err, pp->data,
				      &pp->children[i].data))
			result = 1;

		argv_array_clear(&pp->children[i].process.args);
		argv_array_clear(&pp->children[i].process.env_array);

		pp->nr_processes--;
		pp->children[i].in_use = 0;
		pp->pfd[i].fd = -1;

		if (i != pp->output_owner) {
			strbuf_addbuf(&pp->buffered_output, &pp->children[i].err);
			strbuf_reset(&pp->children[i].err);
		} else {
			fputs(pp->children[i].err.buf, stderr);
			strbuf_reset(&pp->children[i].err);

			/* Output all other finished child processes */
			fputs(pp->buffered_output.buf, stderr);
			strbuf_reset(&pp->buffered_output);

			/*
			 * Pick next process to output live.
			 * NEEDSWORK:
			 * For now we pick it randomly by doing a round
			 * robin. Later we may want to pick the one with
			 * the most output or the longest or shortest
			 * running process time.
			 */
			for (i = 0; i < n; i++)
				if (pp->children[(pp->output_owner + i) % n].in_use)
					break;
			pp->output_owner = (pp->output_owner + i) % n;
		}
	}
	return result;
}

int run_processes_parallel(int n,
			   get_next_task_fn get_next_task,
			   start_failure_fn start_failure,
			   task_finished_fn task_finished,
			   void *pp_cb)
{
	int i;
	int output_timeout = 100;
	int spawn_cap = 4;
	struct parallel_processes *pp;

	pp = pp_init(n, get_next_task, start_failure, task_finished, pp_cb);
	while (1) {
		for (i = 0;
		    i < spawn_cap && !pp->shutdown &&
		    pp->nr_processes < pp->max_processes;
		    i++) {
			int code = pp_start_one(pp);
			if (!code)
				continue;
			if (code < 0) {
				pp->shutdown = 1;
				kill_children(pp, SIGTERM);
			}
			break;
		}
		if (!pp->nr_processes)
			break;
		pp_buffer_stderr(pp, output_timeout);
		pp_output(pp);
		if (pp_collect_finished(pp)) {
			kill_children(pp, SIGTERM);
			pp->shutdown = 1;
		}
	}

	pp_cleanup(pp);
	return 0;
}
