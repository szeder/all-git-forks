#include "cache.h"
#include "watchman-support.h"
#include "strbuf.h"
#include "dir.h"
#include "exec_cmd.h"
#include <watchman.h>
#include <sys/file.h>

static int core_lockout_watchman;

extern void git_config_nodie(config_fn_t fn, void *);

static struct watchman_query *make_query(const char *last_update)
{
	struct watchman_query *query = watchman_query();
	watchman_query_set_fields(query, WATCHMAN_FIELD_NAME |
					 WATCHMAN_FIELD_EXISTS |
					 WATCHMAN_FIELD_NEWER);
	watchman_query_set_empty_on_fresh(query, 1);
	query->sync_timeout = core_watchman_sync_timeout;
	if (*last_update)
		watchman_query_set_since_oclock(query, last_update);
	return query;
}

static struct watchman_query_result* query_watchman(
	struct index_state *istate, struct watchman_connection *connection,
	const char *fs_path, const char *last_update)
{
	struct watchman_error wm_error;
	struct watchman_query *query;
	struct watchman_expression *expr;
	struct watchman_query_result *result;

	query = make_query(last_update);
	expr = watchman_true_expression();
	result = watchman_do_query(connection, fs_path, query, expr, &wm_error);
	watchman_free_query(query);
	watchman_free_expression(expr);

	if (!result)
		warning("Watchman query error: %s (at %s)",
			wm_error.message,
			*last_update ? last_update : "the beginning");

	return result;
}

static void update_index(struct index_state *istate,
			 struct watchman_query_result *result)
{
	int i;

	if (result->is_fresh_instance) {
		/* let refresh clear them later */
		for (i = 0; i < istate->cache_nr; i++)
			istate->cache[i]->ce_flags |= CE_WATCHMAN_DIRTY;
		goto done;
	}

	for (i = 0; i < result->nr; i++) {
		struct watchman_stat *wm = result->stats + i;
		int pos;

		if (S_ISDIR(wm->mode) ||
		    !strncmp(wm->name, ".git/", 5) ||
		    strstr(wm->name, "/.git/"))
			continue;

		pos = index_name_pos(istate, wm->name, strlen(wm->name));
		if (pos < 0) {
			if (istate->untracked) {
				char *name = xstrdup(wm->name);
				char *dname = dirname(name);

				/*
				 * dirname() returns '.' for the root,
				 * but we call it ''.
				 */
				if (dname[0] == '.' && dname[1] == 0)
					string_list_append(&istate->untracked->invalid_untracked, "");
				else
					string_list_append(&istate->untracked->invalid_untracked,
							   dname);
				free(name);
			}
			continue;
		}
		/* FIXME: ignore staged entries and gitlinks too? */

		istate->cache[pos]->ce_flags |= CE_WATCHMAN_DIRTY;
	}

done:
	free(istate->last_update);
	istate->last_update    = xstrdup(result->clock);
	istate->cache_changed |= WATCHMAN_CHANGED;
	if (istate->untracked)
		string_list_remove_duplicates(&istate->untracked->invalid_untracked, 0);
}

int check_watchman(struct index_state *istate)
{
	struct watchman_error wm_error;
	struct watchman_connection *connection;
	struct watchman_query_result *result;
	const char *fs_path;
	struct timeval timeout;
	/*
	 * Convert core_watchman_sync_timeout, in milliseconds, to
	 * struct timeval, in seconds and microseconds.
	 */

	fs_path = get_git_work_tree();
	if (!fs_path)
		return -1;

	timeout.tv_sec = core_watchman_sync_timeout / 1000;
	timeout.tv_usec = (core_watchman_sync_timeout % 1000) * 1000;
	connection = watchman_connect(timeout, &wm_error);

	if (!connection) {
		warning("Watchman watch error: %s", wm_error.message);
		return -1;
	}

	if (watchman_watch(connection, fs_path, &wm_error)) {
		warning("Watchman watch error: %s", wm_error.message);
		watchman_connection_close(connection);
		return -1;
	}


	result = query_watchman(istate, connection, fs_path, istate->last_update);
	watchman_connection_close(connection);
	if (!result)
		return -1;
	update_index(istate, result);
	watchman_free_query_result(result);
	return 0;
}

/**
 * run a backgrounded watchman
 */
static void run_watchman(const char* dir, int pidfd, const char* socket_path)
{
	pid_t pid;
	FILE *pidfile;

	/* paths */
	char watchman_path_buffer[PATH_MAX];
	char log_path[PATH_MAX];
	char state_path[PATH_MAX];
	char net_log_path[PATH_MAX];
	char inner_socket_path[PATH_MAX];
	const char *do_trace;
	char* path_to_watchman = NULL;

	/* If there is a watchman executable sitting side-by-side with the current
	 * binary, then we prefer to use that.  Otherwise we'll use whatever
	 * is in the path */
	char* exe_path = get_executable_path();
	if (exe_path != NULL) {
		struct stat sb;
		int res;

		char* slash = git_find_last_dir_sep(exe_path);
		if (slash != NULL) {
			int path_len = slash - exe_path + 1;
			char* end = stpncpy(watchman_path_buffer, exe_path, path_len);
			strncpy(end, "watchman", PATH_MAX - path_len);

			res = lstat(watchman_path_buffer, &sb);
			if (res == 0 && S_ISREG(sb.st_mode) && (sb.st_mode & S_IXUSR) != 0) {
				path_to_watchman = watchman_path_buffer;
			}
		}
	}

	snprintf(log_path, PATH_MAX, "%s/watchman.log", dir);
	snprintf(state_path, PATH_MAX, "%s/watchman.state", dir);
	snprintf(net_log_path, PATH_MAX, "%s/watchman-net-trace.log", dir);

	do_trace = getenv("WATCHMAN_TRACE");

	if (do_trace && *do_trace) {
		snprintf(inner_socket_path, PATH_MAX, "%s.0", socket_path);
	} else {
		strncpy(inner_socket_path, socket_path, PATH_MAX-1);
	}

	pid=fork();
	switch (pid) {
		int log_fd, devnull_fd;
		case 0:
			/* it's ok if the files aren't there */
			if (0 != unlink(socket_path) && errno != ENOENT)
				die_errno("could not unlink %s", socket_path);
			if (0 != unlink(inner_socket_path) && errno != ENOENT)
				die_errno("could not unlink %s", inner_socket_path);

			if (setsid() == -1)
				die_errno("setsid failed");
			if (0 > (log_fd = open(log_path, O_WRONLY|O_APPEND|O_CREAT, 0644)))
				die_errno("could not open watchman log %s\n", log_path);
			dup2(log_fd, 1);
			dup2(log_fd, 2);
			if (0 > (devnull_fd = open("/dev/null", O_RDONLY)))
				die_errno("could not open devnull for watchman\n");
			dup2(devnull_fd, 0);

			if (NULL != path_to_watchman) {
				execl(path_to_watchman, path_to_watchman,
				      "-f", "-U", inner_socket_path,
							"-o", log_path,
							"--statefile", state_path,
							NULL);
			} else {
				execlp("watchman", "watchman",
				       "-f", "-U", inner_socket_path,
							"-o", log_path,
				       "--statefile", state_path,
							 NULL);
			}
			die_errno("exec failed");
			break;
		case -1:
			die_errno("couldn't fork");
			break;
		default:
			pidfile = fdopen(pidfd, "wb");
			assert(pidfile);
			fprintf(pidfile, "%"PRIuMAX, (uintmax_t)pid);
			fclose(pidfile);

			break;
	}
	/* use socat to debug watchman */
	if (do_trace) {
		char arg1[256], arg2[256];
		switch (fork()) {
			case 0:
				if (setsid() == -1)
					die_errno("setsid failed");
				if (NULL == freopen(net_log_path, "w", stdout))
					die_errno("could not open watchman log\n");
				if (NULL == freopen(net_log_path, "w", stderr))
					die_errno("could not open watchman log\n");
				if (NULL == freopen("/dev/null", "r", stdin))
					die_errno("could not reopen watchman log\n");
				snprintf(arg1, sizeof(arg1), "UNIX-LISTEN:%s,mode=700,reuseaddr,fork", socket_path);
				snprintf(arg2, sizeof(arg2), "UNIX-CONNECT:%s", inner_socket_path);
				execlp("socat", "socat", "-t60", "-x", "-v", arg1, arg2, NULL);
				die_errno("exec failed");
				break;
			case -1:
				die_errno("couldn't fork");
				break;
			default:
				break;
		}
	}
}

/**
 * If the watchan pointed to by the given pidfile is running, returns -1.
 * Else, returns the fd of the watchman pidfile.
 */
static int check_watchman_running(const char *pidfile_path)
{
	int pidfd = open(pidfile_path, O_RDWR|O_CREAT, 0644);
	char pidbuf[64] = {0};

	if (pidfd < 0) {
		die_errno("could not open watchman pidfile\n");
	}

	if (flock(pidfd, LOCK_EX|LOCK_NB)) {
		if (errno == EWOULDBLOCK) {
			close(pidfd);
			return -1;
		} else {
			die_errno("could not flock watchman pidfile\n");
		}
	}

	if (0 <= read_in_full(pidfd, pidbuf, sizeof(pidbuf))) {
		pid_t pid = strtol(pidbuf, (char **)NULL, 10);
		if (pid > 0 && kill(pid, 0) == 0) {
			/* This case should never happen; watchman should
			   hold the pidfile lock. */
			close(pidfd);
			return -1;
		}
	} else {
		die_errno("could not read pidfile\n");
	}

	lseek(pidfd, 0, SEEK_SET);

	return pidfd;
}

/**
 * custom config parser that only cares about watchman and does not die
 * if the config file has errors etc.  Otherwise we might die before invoking
 * config-repairing git-config commands.
 */
static int git_watchman_config(const char *var, const char *value, void *cb)
{
	if (!strcmp(var, "core.lockoutwatchman")) {
		core_lockout_watchman = git_config_bool(var, value);
		return 0;
	}

	if (!strcmp(var, "core.usewatchman")) {
		const char* disable_env = getenv("GIT_DISABLE_WATCHMAN");
		if (disable_env) {
			core_use_watchman = !git_config_bool(var, disable_env);
		} else {
			core_use_watchman = git_config_bool(var, value);
		}
		return 0;
	}

	if (!strcmp(var, "core.watchmansynctimeout")) {
		core_watchman_sync_timeout = git_config_int(var, value);
		return 0;
	}

	/* copypasta from config.c */
	if (!strcmp(var, "core.excludesfile"))
		return git_config_pathname(&excludes_file, var, value);

	return 0;
}

/**
 * check if watchman is running, and if not, spawn it
 */
void check_run_watchman(void)
{
	git_config(git_watchman_config, NULL);
	if (core_use_watchman && !core_lockout_watchman) {
		char socket_path[PATH_MAX];
		char pidfile_path[PATH_MAX];
		char watchmandir_path[PATH_MAX];
		const char *homedir = getenv("WATCHMAN_HOME");
		int pidfd;

		if (!homedir)
			homedir = getenv("HOME");
		assert(homedir);

		snprintf(watchmandir_path, PATH_MAX, "%s/.watchman", homedir);
		if (mkdir(watchmandir_path, 0755)) {
			if (errno != EEXIST) {
				die_errno("could not make ~/.watchman dir");
			}
		}


		snprintf(pidfile_path, PATH_MAX, "%s/watchman.pid", watchmandir_path);
		snprintf(socket_path, PATH_MAX, "%s/watchman.sock", watchmandir_path);
		if (0 > setenv("WATCHMAN_SOCK", socket_path, 1)) {
			die_errno("could not set WATCHMAN_SOCK environment variable");
		}

		if (0 < (pidfd = check_watchman_running(pidfile_path))) {
			run_watchman(watchmandir_path, pidfd, socket_path);
		}
	}
}
