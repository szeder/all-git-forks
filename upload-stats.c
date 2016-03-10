/*
 * Builtin "git upload-stats".
 *
 * Copyright (c) 2015 Twitter, Inc.
 */

#include "cache.h"
#include "dir.h"
#include "builtin.h"
#include "parse-options.h"
#include "exec_cmd.h"
#include "stats-report.h"
#include "twitter-config.h"
#include "lockfile.h"

#include <curl/curl.h>
#include <sys/file.h>

static char const * const upload_stats_usage[] = {
	N_("git upload-stats [-v] [--url <url>]"),
	NULL
};

static size_t discard_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	return size * nmemb;
}

static int upload_stats_file(CURL *curl, const char *server_url, const char *filename)
{
	CURLcode result = -1;
	struct curl_httppost *formpost=NULL;
	struct curl_httppost *lastptr=NULL;

	/* Fill in the form upload of the stats file */
	curl_formadd(&formpost,
               &lastptr,
               CURLFORM_COPYNAME, "statslog",
               CURLFORM_FILE, filename,
               CURLFORM_END);

	if (getenv("GIT_CURL_VERBOSE"))
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_data);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_URL, server_url);
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

	result = curl_easy_perform(curl);
	trace_printf_key(&stats_trace, "upload-stats: curl returned: %s\n", curl_easy_strerror(result));

	if(result != CURLE_OK) {
		openlog("twgit-stats-report", LOG_CONS, LOG_USER);
		syslog(LOG_WARNING, "%s", curl_easy_strerror(result));
	}
	curl_formfree(formpost);
	return result;
}

/**
 * Walk the stats dir looking for chunk files and uploads them, deleting them
 * if the upload is successful, or just bailing if any attempt fails.
 */
static int upload_chunks(CURL *curl, const char *server_url)
{
	DIR* dirp;
	struct dirent *dirent;
	int ret = 0;
	char *stats_dir_path = expand_user_path(STATS_DIR);
	assert(stats_dir_path);

	if (NULL == (dirp = opendir(stats_dir_path))) {
		trace_printf_key(&stats_trace, "upload-stats: could not open stats dir: %s\n", strerror(errno));
		return -1;
	}

	while (NULL != (dirent = readdir(dirp))) {
		if (0 == strncmp(dirent->d_name, "chunk", strlen("chunk"))) {
			const char * chunk_path = mkpathdup("%s/%s", stats_dir_path, dirent->d_name);
			assert(chunk_path);
			if (CURLE_OK != upload_stats_file(curl, server_url, chunk_path)) {
				ret = -1; // ok something went wrong but let's try the other chunks
			}
			else {
				unlink(chunk_path);
			}

			free((void*)chunk_path);
		}
	}

	closedir(dirp);
	free(stats_dir_path);
	return ret;
}

/**
 * Grab a lockfile to ensure atomicity and then proceed to upload all the things
 */
static void upload_saved_stats(const char *server_url)
{
	CURL *curl;
	char *lastupload_path = expand_user_path(STATS_LASTUPLOAD);
	const char *stats_log = expand_user_path(STATS_LOG);
	const char *stats_lock = expand_user_path(STATS_LOCK);
	int lockfd;

	assert(lastupload_path);
	assert(stats_log);
	assert(stats_lock);

	/** best-effort to open/lock/rename the stats log, but it's not fatal if we can't */
	if (0 > (lockfd = open(stats_lock, O_CREAT | O_RDWR, STATS_FILEMASK))) {
		trace_printf_key(&stats_trace, "Couldn't open the stats lock: %s\n", strerror(errno));
	}
	else {
		if (!flock(lockfd, LOCK_EX|LOCK_NB)) {
			rename_log_to_chunk(stats_log);
		}
		else if (errno == EWOULDBLOCK) {
			trace_printf_key(&stats_trace, "Couldn't lock the stats lock\n");
		}
		else {
			die_errno("Error locking stats lock\n");
		}
		close(lockfd);
	}

	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();
	if (!curl) {
		trace_printf_key(&stats_trace, "upload-stats: curl_easy_init failed\n");
		goto done;
	}

	/** prevent simultaneous uploads **/
	if (0 > (lockfd = open(lastupload_path, O_CREAT|O_RDWR, STATS_FILEMASK))) {
		trace_printf_key(&stats_trace, "upload-stats: couldn't open upload lock.\n");
		goto done;
	}

	if (!flock(lockfd, LOCK_EX|LOCK_NB)) {
		/* modify the file before we upload */
		if (write_in_full(lockfd, "start", 6) != 6)
			die("failed to write");
		if (0 == upload_chunks(curl, server_url)) {
			trace_printf_key(&stats_trace, "upload-stats: upload successful\n");
		}
		/* update the timestamp after we upload */
		if (write_in_full(lockfd, "end", 4) != 4)
			die("failed to write");
	}
	else {
		trace_printf_key(&stats_trace, "upload-stats: couldn't lock lastupload\n");
	}

	close(lockfd);

done:

	if (curl) {
		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();
	free(lastupload_path);
	free((char *)stats_log);
	free((char *)stats_lock);
}

int main(int argc, const char **argv)
{
	const char *url;
	struct option opts[] = {
		OPT_STRING(0, "url", &url, N_("url"), N_("the remote url to post stats to")),
		OPT_END(),
	};
	git_extract_argv0_path(argv[0]);

	reset_stats_report(); /* stats uploads should not be tracked */

	git_config(git_twitter_config, NULL); /* read config to get stats url */
	url = stats_url;

	argc = parse_options(argc, argv, NULL, opts, upload_stats_usage, 0);

	trace_printf_key(&stats_trace, "upload-stats: Target url is: %s\n", url);

	/* we do not take arguments other than flags for now */
	if (argc)
		usage_with_options(upload_stats_usage, opts);

	upload_saved_stats(url);

	return 0;
}
