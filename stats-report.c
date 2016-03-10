#include <arpa/inet.h>
#include <dirent.h>
#include <ifaddrs.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include "breakpad.h"
#include "cache.h"
#include "exec_cmd.h"
#include "git-compat-util.h"
#include "remote.h"
#include "twitter-config.h"
#include "stats-report.h"
#include "version.h"
#include "zipkin.h"
#include "refs.h"
#include "lockfile.h"

#include <jansson.h>

#define UPLOAD_INTERVAL 300 /* five minutes */
// 2.5 megs in size
#define MAX_CHUNK_SIZE 2500000
#define CHUNK_TEMPLATE STATS_DIR"/chunk_XXXXXX"

/* twitter config variables */
extern const char *stats_url;
extern int stats_enabled;

extern int get_parent_proc(char **cmd, char ***cmdline);

struct trace_key stats_trace = TRACE_KEY_INIT(STATS);

/* have we recorded a stats dump yet? */
int stats_recorded = 0;

void bytes_to_hex(char *out, const char *data, size_t len);

struct gitstats stats = {0};

double timeval_double(struct timeval *t)
{
	return (double)t->tv_sec + (double)t->tv_usec/1000000;
}

static json_t *json_string_or_null(const char *str)
{
	if (str)
		return json_string(str);
	else
		return json_null();
}

static json_t* json_rusage(struct rusage *rusage)
{
	json_t *rusage_json = json_object();

	double utime = timeval_double(&rusage->ru_utime);
	double stime = timeval_double(&rusage->ru_stime);

	json_object_set_new(rusage_json, "ru_utime", json_real(utime));	/* user time used */
	json_object_set_new(rusage_json, "ru_stime", json_real(stime));	/* system time used */
	json_object_set_new(rusage_json, "ru_maxrss", json_integer(rusage->ru_maxrss));	/* max resident set size */
	json_object_set_new(rusage_json, "ru_ixrss", json_integer(rusage->ru_ixrss));	/* integral shared text memory size */
	json_object_set_new(rusage_json, "ru_idrss", json_integer(rusage->ru_idrss));	/* integral unshared data size */
	json_object_set_new(rusage_json, "ru_isrss", json_integer(rusage->ru_isrss));	/* integral unshared stack size */
	json_object_set_new(rusage_json, "ru_minflt", json_integer(rusage->ru_minflt));	/* page reclaims */
	json_object_set_new(rusage_json, "ru_majflt", json_integer(rusage->ru_majflt));	/* page faults */
	json_object_set_new(rusage_json, "ru_nswap", json_integer(rusage->ru_nswap));	/* swaps */
	json_object_set_new(rusage_json, "ru_inblock", json_integer(rusage->ru_inblock));	/* block input operations */
	json_object_set_new(rusage_json, "ru_oublock", json_integer(rusage->ru_oublock));	/* block output operations */
	json_object_set_new(rusage_json, "ru_msgsnd", json_integer(rusage->ru_msgsnd));	/* messages sent */
	json_object_set_new(rusage_json, "ru_msgrcv", json_integer(rusage->ru_msgrcv));	/* messages received */
	json_object_set_new(rusage_json, "ru_nsignals", json_integer(rusage->ru_nsignals));	/* signals received */
	json_object_set_new(rusage_json, "ru_nvcsw", json_integer(rusage->ru_nvcsw));	/* voluntary context switches */
	json_object_set_new(rusage_json, "ru_nivcsw", json_integer(rusage->ru_nivcsw));	/* involuntary context switches */

	return rusage_json;
}

static int json_git_config_helper(const char *key, const char *value, void *array)
{
	json_t *config = (json_t *)array;
	json_t *config_item = json_object();
	json_object_set_new(config_item, "key", json_string(key));
	json_object_set_new(config_item, "value", json_string(value));
	json_array_append_new(config, config_item);
	return 0;
}

static json_t *json_git_config(void)
{
	json_t *config = json_array();

	struct git_config_source config_source;
	memset(&config_source, 0, sizeof(struct git_config_source));

	git_config_with_options(json_git_config_helper, config, &config_source, 0);

	return config;
}

static json_t* json_stats(struct gitstats *stats)
{
	json_t *stats_json = json_object();
	json_t *cmd_line = json_array();
	json_t *parent_cmd_line = json_array();
	char **arg;

	json_object_set_new(stats_json, "rusage", json_rusage(&stats->rusage));

	/* only dump the full config on the base span to avoid redundancy */
	if (TRACE.trace_id == TRACE.span_id) {
		json_object_set_new(stats_json, "config", json_git_config());
	}

	double timestamp = timeval_double(&stats->start);
	double duration = timeval_double(&stats->stop) - timestamp;

	json_object_set_new(stats_json, "timestamp", json_real(timestamp));
	json_object_set_new(stats_json, "duration", json_real(duration));
	json_object_set_new(stats_json, "hostname", json_string(stats->hostname));
	json_object_set_new(stats_json, "ip_address", json_integer(stats->in_addr));
	json_object_set_new(stats_json, "user", json_string(stats->user));
	json_object_set_new(stats_json, "sysname", json_string(stats->uname.sysname));

	for (arg = stats->cmd_line; *arg; ++arg) {
		json_array_append_new(cmd_line, json_string(*arg));
	}
	if (stats->parent_cmdline) {
		for (arg = stats->parent_cmdline; *arg; ++arg) {
			json_array_append_new(parent_cmd_line, json_string(*arg));
		}
	}

	json_object_set_new(stats_json, "cmd_line", cmd_line);
	json_object_set_new(stats_json, "cmd", json_string(stats->cmd));
	json_object_set_new(stats_json, "parent_cmd_line", parent_cmd_line);
	json_object_set_new(stats_json, "parent_cmd", json_string(stats->parent_cmd));
	json_object_set_new(stats_json, "exit_code", json_integer(stats->exit_code));

	json_object_set_new(stats_json, "repo", json_string_or_null(stats->repo));
	json_object_set_new(stats_json, "path", json_string_or_null(stats->path));
	json_object_set_new(stats_json, "branch", json_string_or_null(stats->branch));
	json_object_set_new(stats_json, "commit_sha1", json_string_or_null(stats->commit_sha1));
	json_object_set_new(stats_json, "is_workdir", json_integer(stats->is_workdir));
	json_object_set_new(stats_json, "fetch_transactions", json_integer(stats->fetch_transactions));
	json_object_set_new(stats_json, "watchman_state", json_integer(stats->watchman_state));

	json_object_set_new(stats_json, "version", json_string(stats->version));

	json_object_set_new(stats_json, "zipkin", trace_to_json());

	return stats_json;
}

static int dump_to_file(const char *buffer, size_t size, void *data)
{
	FILE *file = (FILE *)data;

	if (1 != fwrite(buffer, size, 1, file))
		return -1;
	return 0;
}

static void save_stats(struct gitstats *stats)
{
	const char *stats_dir = expand_user_path(STATS_DIR);
	const char *stats_log = expand_user_path(STATS_LOG);
	const char *stats_lock = expand_user_path(STATS_LOCK);
	FILE *logfile = NULL;
	int lockfile_fd=-1;
	off_t offset;
	mode_t mask;

	json_t *json = json_stats(stats);
	assert(json);
	assert(stats_log);
	assert(stats_dir);
	assert(stats_lock);

	if (!stats_dir) {
		 // this can happen if $HOME is unset or other weirdness
		fprintf(stderr, "Could not determine stats dir\n");
		return;
	}

	mkdir(stats_dir, STATS_DIRMASK); /* no harm in trying redundantly */
	chmod(stats_dir, STATS_DIRMASK); /* mkdir won't set the setgid bit for some reason */

	/* hold the lock throughout the critical section
	 * we're going to be renaming the log file, so other processes
	 * will need to wait until that file is renamed so that the
	 * fopen() below creates a new log if necessary. Just holding
	 * a lock on stats.log itself is insufficient. */
	lockfile_fd = open(stats_lock, O_CREAT | O_RDWR, STATS_FILEMASK);
	if (0 > lockfile_fd) {
		fprintf(stderr, "could not open stats lockfile\n");
		goto done;
	}

	if (flock(lockfile_fd, LOCK_EX)) {
		fprintf(stderr, "Could not lock stats lockfile\n");
		goto done;
	}

	mask = umask(0777 - STATS_FILEMASK);
	logfile = fopen(stats_log, "a");
	umask(mask);
	if (!logfile) {
		fprintf(stderr,"Could not fopen stats log\n");
		goto done;
	}

	if (json_dump_callback(json, dump_to_file, (void*)logfile, JSON_COMPACT)) {
		fprintf(stderr, "Could not dump stats file to log\n");
		goto done;
	}

	fputc('\n', logfile); /* terminate with newline */

	if (0 > (offset = ftello(logfile))) {
		fprintf(stderr,"Could not determine stats_log offset");
		goto done;
	}

	if (offset > MAX_CHUNK_SIZE) {
		rename_log_to_chunk(stats_log);
	}

done:

	if (logfile != NULL)
		fclose(logfile); /* implicit flush/unlock on close */
	if (lockfile_fd > 0)
		close(lockfile_fd);
	json_decref(json);
	free((char *)stats_dir);
	free((char *)stats_log);
	free((char *)stats_lock);
}

static char **copy_argv(int argc, const char **argv)
{
	int i;
	char **result = xmalloc((argc + 1) * sizeof(char*));
	for (i = 0; i < argc; ++i) {
		result[i] = strdup(argv[i]);
	}
	result[i] = NULL;
	return result;
}

static void get_git_location(struct gitstats *stats)
{
	static unsigned char commit_sha1[20];
	char *head;
	struct remote *remote;

	stats->repo = NULL;
	remote = remote_get("origin");
	if (remote && remote_is_configured(remote )) {
		if (remote->url_nr)
			stats->repo = remote->url[0];
	}

	if (is_inside_work_tree() && stats->path) {
		static char cwd[PATH_MAX+1];
		if (getcwd(cwd, PATH_MAX) > 0)
			stats->path = prefix_path_gently(cwd, strlen(cwd), NULL, stats->path);
	} else
		stats->path = NULL;

	head = resolve_refdup("HEAD", 0, commit_sha1, NULL);
	if (!head)
		return;
	stats->commit_sha1 = strdup(sha1_to_hex(commit_sha1));
	stats->branch = head;
}

/**
 * assumes we're in the base directory of hte repo.
 * returns 1 if we're in a workdir, 0 if not, and -1 if an error/unknown
 */
static int is_workdir(void)
{
	struct stat s;

	if (0 == lstat(".git/refs", &s)) {
		if (s.st_mode & S_IFLNK)
			return 1;
		if (s.st_mode & S_IFDIR)
			return 0;
	}

	return -1;
}

/**
 * Check to see if any upload has happened recently by touching a
 * lockfile.  This works as a rate-limiter for uploading stats so we
 * don't hammer the server.
 *
 * Returns 1 if no recent upload has occurred.  0 means an upload has
 * been done recently.
 */
static int no_recent_upload(void)
{
	int ret=1;
	struct stat st;
	char *stats_dir = expand_user_path(STATS_DIR);
	assert(stats_dir);
	mkdir(stats_dir, STATS_DIRMASK); /* no harm in trying redundantly */
	chmod(stats_dir, STATS_DIRMASK); /* mkdir won't set the setgid for some reason */
	char *path = expand_user_path(STATS_LASTUPLOAD);

	if (0 == stat(path, &st)) {
		/* make sure it's not too old */
		if (time(NULL) - st.st_mtime < UPLOAD_INTERVAL) {
			/* recent upload has occurred, just back off */
			trace_printf_key(&stats_trace, "upload-stats: mtime of %ld too close to current time of %ld.\n", st.st_mtime, time(NULL));
			ret=0;
		}
	}

	/* either the lockfile isn't there or it's old, go for it */
	free(stats_dir);
	free(path);

	return ret;
}

#undef exit
NORETURN void stats_exit(int status)
{
	static int exited=0;
	if (!exited) {
		exited=1;
		finish_stats_report(status);
	}
	else {
		trace_printf_key(&stats_trace, "stats_exit: warning! recursive exit called.\n");
	}

	exit(status);
}

/**
 * given the stats file name, lock it and move it to a random
 * chunk file name.  The stats file probably should be pre-locked.
 */
int rename_log_to_chunk(const char* stats_file)
{
	int tempfd, ret=0;
	char *chunk_name = expand_user_path(CHUNK_TEMPLATE);
	assert(chunk_name);

	tempfd = mkstemp(chunk_name);
	if (tempfd < 0) {
		fprintf(stderr, "Could not create chunk file\n");
		return -1;
	}

	if (0 > rename(stats_file, chunk_name)) {
		fprintf(stderr, "Could not move stats file over to chunk file\n");
		ret=-1;
	}

	close(tempfd);
	free(chunk_name);

	return ret;
}


/** record stats at the end of the program execution */
void finish_stats_report(int status)
{
	if (stats.state != STATS_INITIALIZED) {/* maybe stats weren't enabled? */
		if (stats.state == STATS_UNINITIALIZED) {
			trace_printf_key(&stats_trace, "Not posting stats because they were never recorded?\n");
		}
		return;
	}

	stats.state = STATS_DUMPED; /* do this early to prevent recursion */

	if (startup_info && !startup_info->have_repository) {
		trace_printf_key(&stats_trace, "git command didn't result in a git repository?\n");
		stats.repo = NULL;
		stats.path = NULL;
		stats.branch = NULL;
		stats.commit_sha1 = NULL;
	} else {
		/*
		 * This data is only relevant if we are inside a git
		 * repo.
		 */
		get_git_location(&stats);
	}

	stats.is_workdir = is_workdir();

	git_config(git_twitter_config, NULL); /* read config to get stats dir and url */

	if (!stats_enabled) {
		trace_printf_key(&stats_trace, "stats_report: config variable is not set, not saving stats\n");
		return;
	}

	gettimeofday(&stats.stop, NULL);

	if (WIFEXITED(status)) {
		stats.exit_code = WEXITSTATUS(status);
	} else {
		stats.exit_code = 128 + WTERMSIG(status);
	}

	getrusage(RUSAGE_SELF, &stats.rusage);

	save_stats(&stats); /* google "osx fork eintr" for why we do the I/O before forking */

	if (no_recent_upload()) { /* we can check before fork because it's fast */

		/* In order to prevent recursion, we set
		 * GIT_DISABLE_STATS, so that any upload-stats
		 * processes that we launch don't themselves cause
		 * stats reporting recursion.
		 */
		setenv("GIT_DISABLE_STATS", "1", 1);

		const char *args[] = {"upload-stats", NULL};
		trace_printf_key(&stats_trace, "stats_report: about to fork/exec upload-stats\n");
		fflush(NULL); // flush everything so we don't get dupes

		switch (fork()) {
			case 0:
				daemonize(NULL);
				execv_git_cmd(args);
				break;
			case -1:
				die_errno("fork failed");
			default: /* continue on our normal exit behavior */
				break;
		}
	}
}

static void stats_atexit(void)
{
	finish_stats_report(0);
}

void reset_stats_report(void)
{
	memset(&stats, 0, sizeof(struct gitstats));
}

/**
 * janky way to get our current ip.  Iterate over all interfaces and pick the first
 * one with a netmask small enough to indicate it's not loopbackish
 */
int32_t get_ipaddress(void)
{
	struct ifaddrs *ifs, *curif;
	int32_t addr=0;

	if (0 != getifaddrs(&ifs)) return 0;

	for (curif=ifs; curif != NULL; curif=curif->ifa_next) {
		if (curif->ifa_addr && curif->ifa_addr->sa_family == AF_INET) {
			uint32_t mask = ((struct sockaddr_in*)curif->ifa_netmask)->sin_addr.s_addr;
			if (mask != 255) {
				// non class-A netmask must be a real network
				addr = ((struct sockaddr_in *)curif->ifa_addr)->sin_addr.s_addr;
			}
		}
	}

	if (ifs != NULL) freeifaddrs(ifs);
	return addr;
}


/* Report the timing of this git command */
void init_stats_report(const char *orig_cmd, int orig_argc, const char** orig_argv)
{
	/* this is the only way to truly bail early since we have to cd to
		 the root to parse the config.  Because of this, we save the
		 config parsing until later. */
	char *disable = getenv("GIT_DISABLE_STATS");
	if (disable && (*disable == '1' || *disable == 't')) {
		trace_printf_key(&stats_trace, "Disabling stats due to GIT_DISABLE_STATS=%s\n", disable);
		return;
	}

	/* get the start time before we do anything else */
	gettimeofday(&stats.start, NULL);

	trace_next_id(); /* zipkin */

	stats.version = git_version_string;
	stats.cmd = xstrdup(orig_cmd);
	stats.cmd_line = copy_argv(orig_argc, orig_argv);
	get_parent_proc(&stats.parent_cmd, &stats.parent_cmdline); // ok if this fails

	stats.watchman_state = WATCHMAN_UNKNOWN;

	/* get cwd early before we cd into the repo root */
	stats.path = getcwd(NULL, 0);

	stats.hostname = xmalloc(_POSIX_HOST_NAME_MAX + 1);
	gethostname(stats.hostname, _POSIX_HOST_NAME_MAX);
	stats.hostname[_POSIX_HOST_NAME_MAX] = 0;
	stats.in_addr = get_ipaddress();
	stats.user = getenv("USER");
	uname(&stats.uname);

	stats.state = STATS_INITIALIZED;
	atexit(stats_atexit);
}
