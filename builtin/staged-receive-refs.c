#include "builtin.h"
#include "pack.h"
#include "refs.h"
#include "pkt-line.h"
#include "run-command.h"
#include "exec_cmd.h"
#include "commit.h"
#include "object.h"
#include "remote.h"
#include "connect.h"
#include "transport.h"
#include "string-list.h"
#include "sha1-array.h"
#include "connected.h"
#include "argv-array.h"
#include "url.h"
#include "version.h"
#include "staged-send-refs.h"

static int quiet;
static int atomic_push_all;
static int atomic_push_per_repo;

static const char receive_pack_usage[] = "git staged-receive-refs";

static int switch_to_repo(const char *url) {
	struct gitdb_config_data gitdb_data = {NULL, NULL};
	const char *dir;

	if (!is_url(url))
		die("does not look like a valid url: %s", url);

	dir = git_path_from_url(url);

	if (!dir)
		die("URL dies not have a path component: %s", url);

	if (!enter_repo(dir, 0))
		die("'%s' does not appear to be a git repository", dir);

	if (git_config_from_file(gitdb_config, "config", &gitdb_data))
		die("'%s' does not appear to be a git with db backend", dir);
	if (!gitdb_data.db_repo_name)
		die("'%s' does not have config/db-repo-name", dir);
	db_repo_name = gitdb_data.db_repo_name;
	if (!gitdb_data.db_socket)
		die("'%s' does not have config/db-socket", dir);
	db_socket = gitdb_data.db_socket;

	return 0;
}

static int parse_update_options(const char *feature_list)
{
	if (parse_feature_request(feature_list, "atomic-push-all"))
		atomic_push_all = 1;
	if (parse_feature_request(feature_list, "atomic-push-per-repo"))
		atomic_push_per_repo = 1;
	return 0;
}

static int update_refs_for_repo(struct staged_repo *repo_list,
				struct staged_repo *repo,
				struct transaction *transaction)
{
	struct staged_ref *ref;
	struct strbuf err = STRBUF_INIT;

	for (ref = repo->refs; ref; ref = ref->next) {
		if (!atomic_push_per_repo && !atomic_push_all) {
			transaction = transaction_begin(&err);
			if (!transaction) {
				ref->error_str = xstrdup(err.buf);
				strbuf_release(&err);
				continue;
			}
		}

		if (transaction_update_ref(transaction, ref->name,
				ref->new_sha1, ref->old_sha1, 0,
					    1, "push", &err)) {
			ref->error_str = xstrdup(err.buf);
			strbuf_release(&err);
			if (atomic_push_per_repo) {
				repo_list->error_str = xstrdup(ref->error_str);
				return -1;
			}
			if (atomic_push_all) {
				repo->error_str = xstrdup(ref->error_str);
				return -1;
			}
		}

		if (!atomic_push_per_repo && !atomic_push_all) {
			if (transaction_commit(transaction, &err)) {
				ref->error_str = xstrdup(err.buf);
				strbuf_release(&err);
			}
			transaction_free(transaction);
		}
	}
	return 0;
}

int cmd_staged_receive_refs(int argc, const char **argv, const char *prefix)
{
	int i;
	struct strbuf err = STRBUF_INIT;
	struct transaction *transaction = NULL;
	int line_num = 0;
	struct staged_repo *repo_list = NULL, *repo = NULL;
	struct strbuf rep_buf = STRBUF_INIT;

	packet_trace_identity("staged-receive-refs");

	argv++;
	for (i = 1; i < argc; i++) {
		const char *arg = *argv++;

		if (*arg == '-') {
			if (!strcmp(arg, "--quiet")) {
				quiet = 1;
				continue;
			}
			usage(receive_pack_usage);
		}
	}
	setup_path();

	for (;;) {
		char *line;
		int len;
		char *refname;
		unsigned char old_sha1[20], new_sha1[20];

		line = packet_read_line(0, &len);
		if (!line)
			break;

		if (!line_num++) {
			if (!starts_with(line, "repo ")) {
				die("First line is not a repo line: %s",
				    line);
			}
			parse_update_options(line + strlen(line) + 1);
		}
		if (starts_with(line, "repo ")) {
			switch_to_repo(line + 5);
			repo = add_staged_repo(&repo_list, line + 5);
			continue;
		}
		if (len < 83 ||
		    line[40] != ' ' ||
		    line[81] != ' ' ||
		    get_sha1_hex(line, old_sha1) ||
		    get_sha1_hex(line + 41, new_sha1))
			die("protocol error: expected old/new/ref, got '%s'",
			    line);

		refname = line + 82;
		add_staged_ref(repo, refname, old_sha1, new_sha1);
	}

	if (atomic_push_all) {
		transaction = transaction_begin(&err);
		if (!transaction) {
			repo_list->error_str = xstrdup(err.buf);
			strbuf_release(&err);
			goto finished_push_all;
		}
	}

	for (repo = repo_list; repo; repo = repo->next) {
		switch_to_repo(repo->url);

		if (atomic_push_per_repo) {
			transaction = transaction_begin(&err);
			if (!transaction) {
				repo->error_str = xstrdup(err.buf);
				strbuf_release(&err);
				continue;
			}
		}

		if (update_refs_for_repo(repo_list, repo, transaction)) {
			if (atomic_push_all)
				break;
			if (atomic_push_per_repo) {
				transaction_free(transaction);
				transaction = NULL;
				continue;
			}
		}

		if (atomic_push_per_repo) {
			if (transaction_commit(transaction, &err)) {
				repo->error_str = xstrdup(err.buf);
				strbuf_release(&err);
				transaction_free(transaction);
				transaction = NULL;
				continue;
			}
		}
	}

	if (atomic_push_all) {
		if (!repo_list->error_str)
			if (transaction_commit(transaction, &err)) {
				repo_list->error_str = xstrdup(err.buf);
				strbuf_release(&err);
			}
		transaction_free(transaction);
		transaction = NULL;
	}

	/*
	 * Make sure repo failures are propagated down to each individual
	 * ref error string.
	 */
	for (repo = repo_list; repo; repo = repo->next) {
		struct staged_ref *ref;

		if (atomic_push_all && repo_list->error_str && !repo->error_str)
			repo->error_str = xstrdup(repo_list->error_str);

		for (ref = repo->refs; ref; ref = ref->next) {
			if (ref->error_str)
				continue;
			if (atomic_push_all && repo_list->error_str) {
				ref->error_str = xstrdup(repo_list->error_str);
				continue;
			}
			if (atomic_push_per_repo && repo->error_str) {
				ref->error_str = xstrdup(repo->error_str);
				continue;
			}
		}
	}


	for (repo = repo_list; repo; repo = repo->next) {
		struct staged_ref *ref;

		if (repo->error_str)
			packet_buf_write(&rep_buf, "repo ng %s %s",
					 repo->url, repo->error_str);
		else
			packet_buf_write(&rep_buf, "repo ok %s",
					 repo->url);

		for (ref = repo->refs; ref; ref = ref->next)
			if (ref->error_str)
				packet_buf_write(&rep_buf, "ng %s %s",
					 ref->name, ref->error_str);
			else
				packet_buf_write(&rep_buf, "ok %s",
					 ref->name);
	}
	packet_buf_flush(&rep_buf);
	write_or_die(1, rep_buf.buf, rep_buf.len);
	packet_flush(1);
	strbuf_release(&rep_buf);

 finished_push_all:

	strbuf_release(&err);
	transaction_free(transaction);

	free_staged_repo_list(repo_list);
	return 0;
}
