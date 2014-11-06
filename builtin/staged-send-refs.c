/*
 * ./refsd-tdb /tmp/refsd.socket /tmp /tmp/refsd.log
 * git init-db --db-repo-name=REPO-1 --db-socket=/tmp/refsd.socket --refs-backend-type=db
 * git push --staged-push file:///usr/local/google/home/sahlberg/git-tng/tst tst:tst
 */

#include "builtin.h"
#include "connect.h"
#include "lockfile.h"
#include "pkt-line.h"
#include "run-command.h"
#include "staged-send-refs.h"

static struct lock_file staging_lock;

static const char staged_send_refs_usage[] =
  "git staged-send-refs --staged-receive-refs=<git-staged-receive-refs>] [--verbose] [--list] [--atomic-push-all|--atomic-push-per-repo]\n";

static int verbose;
static int atomic_push_all;
static int atomic_push_per_repo;

static int cmd_unstage_repo(struct staged_repo *repo_list, const char *url)
{
	repo_list = remove_staged_repo(repo_list, url);
	return write_staging_file(&staging_lock, repo_list);
}

static int cmd_list_staged_refs(struct staged_repo *repo_list)
{
	while (repo_list) {
		struct staged_ref *ref;

		printf("repo:%s\n", repo_list->url);
		for (ref = repo_list->refs; ref; ref = ref->next) {
			if (starts_with(ref->name, "refs/staging"))
				continue;
			printf("    ref:%s\n", ref->name);
		}
		repo_list = repo_list->next;
	}
	return 0;
}

int cmd_staged_send_refs(int argc, const char **argv, const char *prefix)
{
	const char *receiverefs = "git-staged-receive-refs";
	struct staged_repo *repo, *repo_list = NULL;
	int list = 0, unstage = 0;
	int ret = 0, i;
	int fd[2];
	struct child_process *conn;
	struct strbuf req_buf = STRBUF_INIT;
	int in, out;
	char *repo_url = NULL;

	argv++;
	for (i = 1; i < argc; i++, argv++) {
		const char *arg = *argv;

		if (*arg == '-') {
			if (starts_with(arg, "--staged-receive-refs=")) {
				receiverefs = arg + 22;
				continue;
			}
			if (starts_with(arg, "--exec=")) {
				receiverefs = arg + 7;
				continue;
			}
			if (!strcmp(arg, "--list")) {
				list = 1;
				continue;
			}
			if (!strcmp(arg, "--unstage")) {
				unstage = 1;
				continue;
			}
			if (!strcmp(arg, "--verbose")) {
				verbose = 1;
				continue;
			}
			if (!strcmp(arg, "--atomic-push-all")) {
				atomic_push_all = 1;
				continue;
			}
			if (!strcmp(arg, "--atomic-push-per-repo")) {
				atomic_push_per_repo = 1;
				continue;
			}
			usage(staged_send_refs_usage);
		}
		if (!repo_url) {
			repo_url = xstrdup(arg);
			continue;
		}
	}

	if (atomic_push_all && atomic_push_per_repo) {
		fprintf(stderr, "--atomic-push-all and --atomic-push-per-repo "
			"are mutually exclusive.\n");
		usage(staged_send_refs_usage);
		ret = -1;
		goto finished;
	}

	if (lock_staging_file_for_update(&staging_lock)) {
		fprintf(stderr, "Failed to lock staging file\n");
		goto finished;
	}
	repo_list = read_staging_file(&staging_lock);
	if (!repo_list) {
		fprintf(stderr, "Nothing to destage\n");
		rollback_lock_file(&staging_lock);
		goto finished;
	}

	if (list)
		return cmd_list_staged_refs(repo_list);
	if (unstage)
		return cmd_unstage_repo(repo_list, repo_url);

	conn = git_connect(fd, repo_list->url, receiverefs,
			   verbose ? CONNECT_VERBOSE : 0);
	in = fd[0];
	out = fd[1];

	for(repo = repo_list; repo; repo = repo->next) {
		struct staged_ref *ref = repo->refs;

		if (repo == repo_list)
			packet_buf_write(&req_buf, "repo %s%c%s",
				 repo->url, 0,
				 atomic_push_all ? "atomic-push-all" :
				 atomic_push_per_repo ? "atomic-push-per-repo" : "");
		else
			packet_buf_write(&req_buf, "repo %s", repo->url);

		for (ref = repo->refs; ref; ref = ref->next)
			packet_buf_write(&req_buf, "%s %s %s",
				    sha1_to_hex(ref->old_sha1),
				    sha1_to_hex(ref->new_sha1),
					 ref->name);
	}

	packet_buf_flush(&req_buf);
	write_or_die(out, req_buf.buf, req_buf.len);
	packet_flush(out);
	strbuf_release(&req_buf);

	for (;;) {
		char *line;
		int len;

		line = packet_read_line(in, &len);
		if (!line)
			break;

		if (starts_with(line, "repo ")) {
			if (repo_url)
				free(repo_url);
			repo_url = xstrdup(line + 8);
			printf("Updates to %s\n", repo_url);
			continue;
		}
		printf("  %s\n", line);
		if (starts_with(line, "ok")) {
			remove_staged_ref(repo_list, repo_url, line + 3);
		}
	}
	write_staging_file(&staging_lock, repo_list);

 finished:
	free(repo_url);
	free_staged_repo_list(repo_list);
	return ret;
}
