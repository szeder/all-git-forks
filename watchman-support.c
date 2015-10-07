#ifndef NO_PTHREADS
#include <pthread.h>
#endif

#include "cache.h"
#include "dir.h"
#include "exec_cmd.h"
#include "fs_cache.h"
#include "strbuf.h"
#include "string-list.h"
#include "pathspec.h"
#include "watchman-support.h"
#include "trace.h"

#include <dirent.h>
#include <pthread.h>
#include <sys/file.h>

#define NS_PER_SEC 1000000000L

static struct trace_key watchman_trace = TRACE_KEY_INIT(WATCHMAN);

static void copy_wm_stat_to_fe(struct watchman_stat *wm, struct fsc_entry *fe)
{
	if (!wm->exists) {
		fe_set_deleted(fe);
		return;
	} else
		fe_clear_deleted(fe);
	fe->st.st_size = wm->size;
	fe->st.st_mode = wm->mode;
	fe->st.st_ino = wm->ino;
	fe->st.st_dev = wm->dev;
	fe->st.st_uid = wm->uid;
	fe->st.st_gid = wm->gid;
	fe->st.st_mtime = wm->mtime_ns / NS_PER_SEC;
	fe->st.st_ctime = wm->ctime_ns / NS_PER_SEC;
	return;
}

static struct fsc_entry *wm_stat_to_fe(struct watchman_stat *wm)
{
	struct fsc_entry *fe = make_fs_cache_entry(wm->name);
	fe_set_new(fe);
	copy_wm_stat_to_fe(wm, fe);
	return fe;
}

static void update_exclude(struct dir_struct *dir, struct fsc_entry *fe)
{
	int dtype = fe_dtype(fe);
	if (is_excluded(dir, fe->path, &dtype)) {
		fe_set_excluded(fe);
	} else {
		fe_clear_excluded(fe);
	}
	for (fe = fe->first_child; fe; fe = fe->next_sibling) {
		update_exclude(dir, fe);
	}
}

static struct fsc_entry *fs_cache_file_deleted(struct watchman_stat *wm)
{
	int namelen = strlen(wm->name);
	struct fsc_entry *fe;

	fe = fs_cache_file_exists(wm->name, namelen);

	if (fe) {
		fe_set_deleted(fe);
		fe_clear_children(fe);
	}

	return fe;
}

static struct fsc_entry *fs_cache_file_modified(struct watchman_stat *wm)
{
	int namelen = strlen(wm->name);
	struct fsc_entry *fe;
	fe = fs_cache_file_exists(wm->name, namelen);
	if (!fe) {
		fe = wm_stat_to_fe(wm);
		fs_cache_insert(fe);
		if (set_up_parent(fe)) {
			fs_cache_remove(fe);
			return NULL;
		}
	} else {
		int was_dir = fe_is_dir(fe);
		if (fe_deleted(fe))
			fe_set_new(fe);
		copy_wm_stat_to_fe(wm, fe);
		if (was_dir && !fe_is_dir(fe)) {
			fe_clear_children(fe);
		}
	}
	return fe;
}

static struct watchman_expression *make_expression(void)
{
	struct watchman_expression *types[3];
	struct watchman_expression *expr;

	types[0] = watchman_type_expression('f');
	types[1] = watchman_type_expression('d');
	types[2] = watchman_type_expression('l');
	expr = watchman_anyof_expression(3, types);

	return expr;
}

struct watchman_query *make_query(const char *last_update)
{
	struct watchman_query *query = watchman_query();

	watchman_query_set_fields(query,
				  WATCHMAN_FIELD_CCLOCK |
				  WATCHMAN_FIELD_OCLOCK |
				  WATCHMAN_FIELD_NAME |
				  WATCHMAN_FIELD_MTIME_NS |
				  WATCHMAN_FIELD_CTIME_NS |
				  WATCHMAN_FIELD_INO |
				  WATCHMAN_FIELD_DEV |
				  WATCHMAN_FIELD_UID |
				  WATCHMAN_FIELD_GID |
				  WATCHMAN_FIELD_EXISTS |
				  WATCHMAN_FIELD_MODE |
				  WATCHMAN_FIELD_SIZE);
	watchman_query_set_empty_on_fresh(query, 1);

	query->sync_timeout = core_watchman_sync_timeout;

	if (last_update) {
		watchman_query_set_since_oclock(query, last_update);
	}
	return query;
}

enum path_treatment {
	path_recurse,
	path_file
};

static void update_all_excludes(void)
{
	struct fsc_entry *root = fs_cache_file_exists("", 0);
	struct dir_struct dir;
	char original_path[PATH_MAX + 1];
	const char *fs_path = get_git_work_tree();

	if (!getcwd(original_path, PATH_MAX + 1))
			die_errno("failed to get working directory\n");
	if (chdir(fs_path))
			die_errno("failed to chdir to git work tree\n");

	assert(root);

	memset(&dir, 0, sizeof(dir));
	setup_standard_excludes(&dir);
	update_exclude(&dir, root);
	clear_directory(&dir);

	if (chdir(original_path))
			die_errno("failed to chdir back to original path\n");
}

static enum path_treatment watchman_handle(struct strbuf *path, struct dirent *de, int rootlen, struct fsc_entry **out)
{
	struct fsc_entry *fe;
	int dtype;

	fe = make_fs_cache_entry(path->buf + rootlen);
	*out = fe;
	fs_cache_insert(fe);
	/* Ignore the return value: set_up_parent will never fail here */
	set_up_parent(fe);
	lstat(path->buf, &fe->st);

	dtype = DTYPE(de);
	if (dtype == DT_UNKNOWN) {
		/* this involves an extra stat call, but only on
		 * Cygwin, which watchman doesn't support anyway. */
		dtype = get_dtype(de, path->buf, path->len);
	}
	if (dtype == DT_DIR) {
		return path_recurse;
	}

	return path_file;
}

static void path_set_last_component(struct strbuf *path, int baselen, const char *add)
{
	strbuf_setlen(path, baselen);
	if (baselen) {
		strbuf_addch(path, '/');
	}
	strbuf_addstr(path, add);
}

static int is_in_watchman_ignore(const char *path)
{
	return unsorted_string_list_has_string(&core_watchman_ignored_dirs,
					       path);
}

static int preload_wt_recursive(struct strbuf *path, int rootlen)
{
	DIR *fdir;
	struct dirent *de;
	int baselen = path->len;

	fdir = opendir(path->buf);
	if (!fdir) {
		return error("Failed to open %s", path->buf);
	}

	while ((de = readdir(fdir)) != NULL) {
		struct fsc_entry *fe;
		if (is_dot_or_dotdot(de->d_name) || is_in_dot_git(de->d_name))
			continue;

		path_set_last_component(path, baselen, de->d_name);
		if (is_in_watchman_ignore(path->buf + rootlen))
			continue;

		/* recurse into subdir if necessary */
		if (watchman_handle(path, de, rootlen, &fe) == path_recurse) {
			int result = preload_wt_recursive(path, rootlen);
			if (result) {
				closedir(fdir);
				return result;
			}
		}
	}

	closedir(fdir);
	return 0;
}

static void init_excludes_config(void)
{
	char *xdg_path;
	if (!excludes_file) {
		home_config_paths(NULL, &xdg_path, "ignore");
		excludes_file = xdg_path;
	}
}

static void compute_sha1(const char *path, unsigned char *sha1)
{
	struct stat st;
	if (stat(path, &st)) {
		memset(sha1, 0, 20);
	} else {
		if (index_path(sha1, path, &st, 0)) {
			memset(sha1, 0, 20);
		}
	}
}

static void init_excludes_files(void)
{
	init_excludes_config();
	if (the_fs_cache.excludes_file) {
		free(the_fs_cache.excludes_file);
	}
	if (excludes_file) {
		the_fs_cache.excludes_file = xstrdup(excludes_file);
		compute_sha1(excludes_file, the_fs_cache.user_excludes_sha1);
	} else {
		the_fs_cache.excludes_file = xstrdup("");
		memset(the_fs_cache.user_excludes_sha1, 0, 20);
	}
	compute_sha1(git_path("info/exclude"), the_fs_cache.git_excludes_sha1);
}

static int git_excludes_file_changed(void)
{
	unsigned char sha1[20];

	compute_sha1(git_path("info/exclude"), sha1);
	if (!hashcmp(the_fs_cache.git_excludes_sha1, sha1))
		return 0;
	hashcpy(the_fs_cache.git_excludes_sha1, sha1);
	return 1;
}

static int user_excludes_file_changed(void)
{
	unsigned char sha1[20] = {0};
	struct stat st;

	init_excludes_config();

	if (!excludes_file) {
		if (strlen(the_fs_cache.excludes_file) == 0) {
			return 0;
		}

		the_fs_cache.excludes_file[0] = 0;
		if (is_null_sha1(the_fs_cache.user_excludes_sha1))
			return 0;

		memset(the_fs_cache.user_excludes_sha1, 0, 20);
		return 1;
	}

	/* A change in exclude filename forces an exclude reload */
	if (!the_fs_cache.excludes_file || strcmp(the_fs_cache.excludes_file, excludes_file)) {
		init_excludes_files();
		return 1;
	}

	if (!strlen(the_fs_cache.excludes_file)) {
		return 0;
	}

	if (stat(excludes_file, &st)) {
		/* There is a problem reading the excludes file; this
		 * could be a persistent condition, so we need to
		 * check if the file is presently marked as invalid */
		if (is_null_sha1(the_fs_cache.user_excludes_sha1))
			return 0;
		else {
			memset(the_fs_cache.user_excludes_sha1, 0, 20);
			return 1;
		}
	}

	if (index_path(sha1, excludes_file, &st, 0)) {
		if (is_null_sha1(the_fs_cache.user_excludes_sha1)) {
			return 0;
		} else {
			memset(the_fs_cache.user_excludes_sha1, 0, 20);
			return 1;
		}
	} else {
		if (!hashcmp(the_fs_cache.user_excludes_sha1, sha1))
			return 0;
		hashcpy(the_fs_cache.user_excludes_sha1, sha1);
		return 1;
	}
}

void create_fs_cache(void)
{
	struct strbuf buf = STRBUF_INIT;
	const char *fs_path = get_git_work_tree();
	struct fsc_entry *root;

	strbuf_addstr(&buf, fs_path);
	clear_fs_cache();
	root = make_fs_cache_entry("");
	root->st.st_mode = 040644;
	fs_cache_insert(root);
	preload_wt_recursive(&buf, buf.len + 1);
	the_fs_cache.fully_loaded = 1;
	the_fs_cache.needs_write = 1;
	strbuf_release(&buf);
}

static void watchman_warning(const char *message, ...)
	__attribute__ ((format(printf, 1, 2)));

static void watchman_warning(const char *message, ...)
{

	va_list argptr;
	struct strbuf msg_buf = STRBUF_INIT;

	openlog("git-watchman", LOG_NDELAY | LOG_PID, LOG_USER);

	va_start(argptr, message);
	vsyslog(LOG_DEBUG, message, argptr);
	va_end(argptr);

	strbuf_add(&msg_buf, "warning: ", 9);
	va_start(argptr, message);
	strbuf_vaddf(&msg_buf, message, argptr);
	va_end(argptr);
	trace_strbuf(&watchman_trace, &msg_buf);
	strbuf_release(&msg_buf);
}

static struct watchman_query_result *watchman_fs_cache_query(struct watchman_connection *connection, const char *fs_path, const char *last_update)
{
	struct watchman_error wm_error;
	struct watchman_expression *expr;
	struct watchman_query *query;
	struct watchman_query_result *result = NULL;
	struct stat st;
	int fs_path_len = strlen(fs_path);
	char *git_path;
	struct timeval timeout;
	/* Convert core_watchman_query_timeout, in milliseconds, to
	 * struct timeval, in seconds and microseconds. */
	timeout.tv_sec = core_watchman_query_timeout / 1000;
	timeout.tv_usec = (core_watchman_query_timeout % 1000) * 1000;

	expr = make_expression();
	query = make_query(last_update);
	if (lstat(fs_path, &st)) {
		return NULL;
	}

	git_path = xmalloc(fs_path_len + 6);
	strcpy(git_path, fs_path);
	strcpy(git_path + fs_path_len, "/.git");

	if (lstat(git_path, &st)) {
		/* Watchman gets confused if we delete the .git
		 * directory out from under it, since that's where it
		 * stores its cookies.  So we'll need to delete the
		 * watch and then recreate it. It's OK for this to
		 * fail, as the watch might have already been
		 * deleted. */
		watchman_watch_del(connection, fs_path, &wm_error);

		if (watchman_watch(connection, fs_path, &wm_error)) {
			watchman_warning("Watchman watch error: %s", wm_error.message);
			goto out;
		}
	}
	result = watchman_do_query_timeout(connection, fs_path, query, expr, &timeout, &wm_error);
	if (!result) {
		watchman_warning("Watchman query error: %s (at %s)", wm_error.message, last_update);
		goto out;
	}
	watchman_free_expression(expr);
	watchman_free_query(query);

out:
	free(git_path);
	return result;
}

/*
 * watchman's clocks are of the form c:[numbers]:[more numbers]:...
 * This function compares two such strings piecewise, numerically,
 * lexicographically -- in other words, c:11:5 > c:5:11, and
 * c:1:1 > c:1.
 */
static int cmp_watchman_clock(const char *clocka, const char *clockb)
{
	const char *starta = clocka + 1; /* skip initial c */
	const char *startb = clockb + 1;

	assert(clocka[0] == 'c');
	assert(clockb[0] == 'c');
	while (starta && startb && *starta && *startb) {
		long na;
		long nb;

		assert(*starta == ':');
		assert(*startb == ':');

		/* skip leading colon */
		starta++;
		startb++;

		errno = 0;
		na = strtol(starta, (char **) &starta, 10);
		if (errno)
			na = -1;

		errno = 0;
		nb = strtol(startb, (char **) &startb, 10);
		if (errno)
			nb = -1;

		if (na > nb)
			return 1;
		else if (nb > na)
			return -1;
	}
	return 0;
}

/*
 * Compare two watchman_stat objects for sorting.  We compare their names
 * case-insensitively; if they are the same, we compare their creation
 * and modification dates to place the earliest first.  We thus process
 * case-duplicates in-order.
 */
static int cmp_stat(const void *a, const void *b)
{
	const struct watchman_stat* sa = a;
	const struct watchman_stat* sb = b;
	const unsigned char* pa = (unsigned char *)sa->name;
	const unsigned char* pb = (unsigned char *)sb->name;
	int cmp = icmp_topo(pa, pb);
	if (cmp == 0) {
		assert(sa->oclock);
		assert(sb->oclock);
		cmp = cmp_watchman_clock(sa->oclock, sb->oclock);
		if (cmp)
			return cmp;
		return cmp_watchman_clock(sa->cclock, sb->cclock);
	}

	return cmp;
}

static void append(struct fsc_entry ***list, int* cap, int* len, struct fsc_entry *entry)
{
	if (*len >= *cap) {
		int sz;
		*cap = *cap ? *cap * 2 : 10;
		sz = *cap * sizeof(**list);
		*list = xrealloc(*list, sz);
	}
	(*list)[(*len)++] = entry;
}

static int is_child_of(struct fsc_entry *putative_child, struct fsc_entry *parent)
{
	while (putative_child) {
		putative_child = putative_child->parent;
		if (putative_child == parent) {
			return 1;
		}
	}
	return 0;
}

static void update_fs_cache(struct watchman_query_result *result, struct fsc_entry ***exclude_dirty, int *len)
{
	struct fsc_entry *fe;
	int i;
	int cap = 0;
	int rootlen;

	assert(len);
	*len = 0;

	assert(exclude_dirty);
	*exclude_dirty = NULL;

	trace_printf_key(&watchman_trace, "Result count is %d with clock: %s\n", result->nr, result->clock);
	rootlen = strlen(get_git_work_tree()) + 1;
	qsort(result->stats, result->nr, sizeof(*result->stats), cmp_stat);

	for (i = 0; i < result->nr; ++i) {
		/*for each result in the changed set, we need to check
		  it against the index and HEAD */

		struct watchman_stat *wm = result->stats + i;

		if (is_in_dot_git(wm->name)) {
			continue;
		}
		if (is_in_watchman_ignore(wm->name + rootlen)) {
			continue;
		}
		trace_printf_key(&watchman_trace, "    %s\n", wm->name);
		the_fs_cache.needs_write = 1;
		if (wm->exists) {
			fe = fs_cache_file_modified(wm);
		} else {
			fe = fs_cache_file_deleted(wm);
		}

		if (fe) {
			if (ends_with(wm->name, "/.gitignore") ||
			    !strcmp(wm->name, ".gitignore")) {
				append(exclude_dirty, &cap, len, fe->parent);
			} else if (fe_new(fe)) {
				append(exclude_dirty, &cap, len, fe);
			}
		}
	}
}

static struct watchman_connection *watchman_connect_with_timeout(struct watchman_error *wm_error)
{
	struct timeval timeout;
	/* Convert core_watchman_sync_timeout, in milliseconds, to
	 * struct timeval, in seconds and microseconds. */
	timeout.tv_sec = core_watchman_sync_timeout / 1000;
	timeout.tv_usec = (core_watchman_sync_timeout % 1000) * 1000;
	return watchman_connect(timeout, wm_error);
}

int watchman_do_recrawl(void)
{
	struct watchman_error wm_error;
	struct watchman_connection *connection;
	int ret=-1;

	const char *fs_path = get_git_work_tree();
	if (!fs_path) {
		watchman_warning("Can't tell watchman to recrawl when we don't know the working tree");
		return -1;
	}

	connection = watchman_connect_with_timeout(&wm_error);
	if (connection) {
		trace_printf_key(&watchman_trace, "Watchman recrawling\n");
		if (0 == watchman_recrawl(connection, fs_path, &wm_error)) {
			ret = 0;
		}
		else {
			watchman_warning("Watchman error recrawling: %s", wm_error.message);
		}
		watchman_connection_close(connection);
	}

	return ret;
}

int watchman_fast_forward_clock(void)
{
	struct watchman_error wm_error;
	struct watchman_connection *connection;
	int ret = -1;

	const char *fs_path = get_git_work_tree();
	if (!fs_path) {
		watchman_warning("Can't update watchman clock when we don't know the working tree");
		return -1;
	}

	connection = watchman_connect_with_timeout(&wm_error);
	if (connection) {
		char *clock = watchman_clock(connection, fs_path, core_watchman_sync_timeout, &wm_error);
		if (clock) {
			the_fs_cache.last_update = xstrdup(clock);
			the_fs_cache.needs_write = 1;
			trace_printf_key(&watchman_trace, "Reset fs_cache clock to: %s\n", clock);
			ret = 0;
		}
		else {
			watchman_warning("Watchman error recrawling: %s", wm_error.message);
		}
		watchman_connection_close(connection);
	}

	return ret;
}

static int watchman_reload_fs_cache(struct fsc_entry ***exclude_dirty, int *len)
{
	struct watchman_error wm_error;
	struct watchman_query_result *result;
	struct watchman_connection *connection;
	int ret = -1;
	const char *fs_path;
	const char *last_update;

	assert(the_fs_cache.last_update);
	assert(exclude_dirty);
	assert(len);

	last_update = the_fs_cache.last_update;

	fs_path = get_git_work_tree();
	if (!fs_path)
		return -1;

	connection = watchman_connect_with_timeout(&wm_error);

	if (!connection) {
		watchman_warning("Watchman watch error: %s", wm_error.message);
		return -1;
	}

	result = watchman_fs_cache_query(connection, fs_path, last_update);
	if (!result) {
		goto done;
	}
	the_fs_cache.last_update = xstrdup(result->clock);

	update_fs_cache(result, exclude_dirty, len);
	watchman_free_query_result(result);
	ret = 0;
done:
	watchman_connection_close(connection);
	return ret;
}

void load_fs_cache(void)
{
	int read = 0;
	read = read_fs_cache();
	if (!read) {
		create_fs_cache();
		the_fs_cache.repo_path = xstrdup(get_git_work_tree());
		init_excludes_files();
		update_all_excludes();
	}
}

static int watchman_load_fs_cache(struct fsc_entry ***exclude_dirty, int *len, unsigned *fscache_created)
{
	struct watchman_error wm_error;
	int ret = -1;
	const char *fs_path;
	char *last_update = NULL;
	char *stored_repo_path = NULL;
	struct watchman_query_result *result;
	struct watchman_connection *connection;

	assert(exclude_dirty);
	assert(len);
	assert(fscache_created);

	fs_path = get_git_work_tree();
	if (!fs_path)
		return -1;

	trace_printf_key(&watchman_trace, "Connecting to watchman\n");
	connection = watchman_connect_with_timeout(&wm_error);

	if (!connection) {
		watchman_warning("Watchman connection error: %s", wm_error.message);
		return -1;
	}

	trace_printf_key(&watchman_trace, "Connected to watchman\n");

	if (watchman_watch(connection, fs_path, &wm_error)) {
		watchman_warning("Watchman watch error: %s", wm_error.message);
		goto done;
	}

	fs_cache_preload_metadata(&last_update, &stored_repo_path);
	trace_printf_key(&watchman_trace, "Preloaded fs_cache metadata\n");
	if (!last_update || strcmp(stored_repo_path, fs_path)) {
		clear_fs_cache();
		/* fs_cache is corrupt, or refers to another repo path;
		 * let's try recreating it. */
		trace_printf_key(&watchman_trace, "fs_cache corrupt or moved; recreating\n");
		if (last_update)
			free(last_update);
		last_update = NULL;
		/* now we continue, because we need to get the
		 * a last-update time from watchman. */
	}
	free(stored_repo_path);

	result = watchman_fs_cache_query(connection, fs_path, last_update);
	if (last_update) {
		free(last_update);
		last_update = NULL;
	}
	if (!result) {
		goto done;
	}
	trace_printf_key(&watchman_trace,
			 "Done querying watchman: is_fresh_instance = %d.\n",
			 result->is_fresh_instance);

	if (result->is_fresh_instance) {
		create_fs_cache();
		*fscache_created = 1;
		trace_printf_key(&watchman_trace, "Recreated fs_cache from scratch\n");
		the_fs_cache.repo_path = xstrdup(fs_path);
	} else {
		if (!fs_cache_is_valid()) {
			read_fs_cache();
			if (!fs_cache_is_valid()) {
				create_fs_cache();
				*fscache_created = 1;
			}
		}
		trace_printf_key(&watchman_trace, "Loaded fs_cache\n");
		update_fs_cache(result, exclude_dirty, len);
		trace_printf_key(&watchman_trace, "Updated fs_cache\n");
	}

	the_fs_cache.needs_write = 1;
	the_fs_cache.last_update = xstrdup(result->clock);

	watchman_free_query_result(result);
	ret = 0;

done:
	watchman_connection_close(connection);
	return ret;
}

/**
 * run a backgrounded watchman
 */
static void run_watchman(const char* dir, int pidfd, const char* socket_path)
{
	pid_t pid;
	FILE *pidfile;

	/* paths */
	char log_path[PATH_MAX];
	char net_log_path[PATH_MAX];
	char inner_socket_path[PATH_MAX];
	const char *do_trace;
	char* path_to_watchman = NULL;

	snprintf(log_path, PATH_MAX, "%s/watchman.log", dir);
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
				      "-f", "-U", inner_socket_path, NULL);
			} else {
				execlp("watchman", "watchman", "-f", "-U", inner_socket_path, NULL);
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
static int check_watchman(const char *pidfile_path)
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

	if (!strcmp(var, "core.watchmanignore")) {
		string_list_append(&core_watchman_ignored_dirs, xstrdup(value));
		return 0;
	}

	if (!strcmp(var, "core.watchmansynctimeout")) {
		core_watchman_sync_timeout = git_config_int(var, value);
		return 0;
	}

	if (!strcmp(var, "core.watchmanquerytimeout")) {
		core_watchman_query_timeout = git_config_int(var, value);
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
		const char *homedir = getenv("HOME");
		int pidfd;
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

		if (0 < (pidfd = check_watchman(pidfile_path))) {
			run_watchman(watchmandir_path, pidfd, socket_path);
		}
	}
}

enum {
	WATCHMAN_FS_CACHE_LOADING = 0,
	WATCHMAN_FS_CACHE_LOADED  = 1
};

struct watchman_load_thread_args {
	volatile long              status;          /* out     */

	/* Exclude state. */
	struct fsc_entry         **exclude_dirty;   /* out     */
	int                        len;             /* out     */
	unsigned                   fscache_created; /* out     */
};

static void watchman_sync_update_excludes(struct fsc_entry **exclude_dirty, int len, unsigned fscache_created)
{
	if (fscache_created) {

		/* From create_fs_cache */
		init_excludes_files();
		update_all_excludes();

	} else {

		/* From update_fs_cache. */

		/* note that we always want to call both of these functions,
		 * since they update the fs_cache's view of files which are
		 * not watched by watchman */
		int user_changed = user_excludes_file_changed();
		int git_changed = git_excludes_file_changed();
		unsigned all_dirty = user_changed || git_changed;

		if (user_changed || git_changed) {
			trace_printf_key(&watchman_trace, "Excludes files changed; need to reparse\n");
		}

		if (all_dirty) {
			update_all_excludes();
		} else if (exclude_dirty) {
			int i;
			struct dir_struct dir;
			struct fsc_entry *last = NULL;
			char original_path[PATH_MAX + 1];

			assert(len > 0);
			qsort(exclude_dirty, len, sizeof(*exclude_dirty), cmp_fsc_entry);

			if (!getcwd(original_path, PATH_MAX + 1))
				die_errno("failed to get working directory\n");

			if (chdir(get_git_work_tree()))
				die_errno("failed to chdir to git work tree\n");

			memset(&dir, 0, sizeof(dir));
			setup_standard_excludes(&dir);

			for (i = 0; i < len; i++) {
				struct fsc_entry *fe = exclude_dirty[i];

				if (i == 0 || !is_child_of(fe, last)) {
					update_exclude(&dir, fe);
					last = fe;
				}
			}
			clear_directory(&dir);
			if (chdir(original_path))
				die_errno("failed to chdir back to original path\n");
		}
	}

	if (exclude_dirty) {
		free(exclude_dirty);
	}
}

static void *watchman_load_thread_entry(void *context)
{
	struct watchman_load_thread_args *args = (struct watchman_load_thread_args *)context;
	int result = 0;
	long status;

	if (the_fs_cache.last_update) {
		result = watchman_reload_fs_cache(&args->exclude_dirty, &args->len);
		/* We don't ever call create_fs_cache in this path. */
		args->fscache_created = 0;
	} else {
		clear_fs_cache();
		result = watchman_load_fs_cache(&args->exclude_dirty, &args->len, &args->fscache_created);
	}

	if (result) {
		clear_fs_cache();
	}

	/*
	 * __sync_val_compare_and_swap below erects a full memory barrier.
	 */
	do {
		status = __sync_val_compare_and_swap(&args->status, WATCHMAN_FS_CACHE_LOADING, WATCHMAN_FS_CACHE_LOADED);
		/*
		 * If we observe args->status' previous value to be
		 * WATCHMAN_FS_CACHE_LOADING, then the CAS has succeeded. This should
		 * always succeed the first time as this thread is the only writer.
		 */
		assert(status == WATCHMAN_FS_CACHE_LOADING);
	} while (status != WATCHMAN_FS_CACHE_LOADING);
	return NULL;
}

void watchman_async_load_fs_cache(void (*continuation)(void *context), void *context)
{
	int async = core_async_fs_cache_load;
	struct watchman_load_thread_args args = {0};

	assert(continuation);

#ifndef NO_PTHREADS
	if (core_async_fs_cache_load) {
		pthread_t watchman_load_thread;
		int result = pthread_create(&watchman_load_thread,
							   NULL,
							   watchman_load_thread_entry,
							   &args);
		if (result) {
			trace_printf_key(&watchman_trace, "Thread creation failed with error %x\n", result);
			async = 0;
		}
	}
#else
	trace_printf_key(&watchman_trace, "This version of git does not support loading the FS cache on a separate thread.\n");
#endif /* !NO_PTHREADS */

	if (!async) {
		/*
		 * Either this platform doesn't support pthreads, or we weren't able
		 * to create one.
		 */
		watchman_load_thread_entry(&args);
	}

	/* Execute overlapping code. */
	continuation(context);

	/* Await completion of the fs cache load. */
#ifndef NO_PTHREADS
	if (async) {
		while (__sync_val_compare_and_swap(&args.status, WATCHMAN_FS_CACHE_LOADED, WATCHMAN_FS_CACHE_LOADED) != WATCHMAN_FS_CACHE_LOADED) ;
	}
#endif /* !NO_PTHREADS */

	/* Execute watchman code that expectes to be executed *after* the index has been loaded. */
	if (the_fs_cache.fully_loaded) {
		watchman_sync_update_excludes(args.exclude_dirty, args.len, args.fscache_created);
	}
}
