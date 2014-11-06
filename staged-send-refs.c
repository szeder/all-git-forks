#include "builtin.h"
#include "connect.h"
#include "lockfile.h"
#include "pkt-line.h"
#include "sideband.h"
#include "run-command.h"
#include "staged-send-refs.h"

void free_staged_ref(struct staged_ref *ref)
{
	free(ref->error_str);
	free(ref->name);
	free(ref);
}

void free_staged_repo(struct staged_repo *item)
{
	while (item->refs) {
		struct staged_ref *tmp_ref = item->refs->next;
		free_staged_ref(item->refs);
		item->refs = tmp_ref;
	}
	free(item->error_str);
	free(item->url);
	free(item);
}

void free_staged_repo_list(struct staged_repo *list)
{
	while(list) {
		struct staged_repo *tmp_repo = list->next;
		free_staged_repo(list);
		list = tmp_repo;
	}
}

struct staged_repo *add_staged_repo(struct staged_repo **repo_list,
				    const char *url)
{
	struct staged_repo *repo;

	for (repo = *repo_list; repo; repo = repo->next)
		if (!strcmp(repo->url, url))
			return repo;

	repo = xmalloc(sizeof(*repo));
	memset(repo, 0, sizeof(*repo));

	repo->url = xstrdup(url);
	repo->next = *repo_list;

	*repo_list = repo;

	return repo;
}

int add_staged_ref(struct staged_repo *repo, const char *name, const unsigned char *old_sha1, const unsigned char *new_sha1)
{
	struct staged_ref *ref;

	ref = xmalloc(sizeof(*ref));
	memset(ref, 0, sizeof(*ref));
	ref->name = xstrdup(name);
	hashcpy(ref->old_sha1, old_sha1);
	hashcpy(ref->new_sha1, new_sha1);

	ref->next = repo->refs;
	repo->refs = ref;

	return 0;
}

struct staged_repo *read_staging_file(struct lock_file *lock)
{
	FILE *f = NULL;
	char line[1024];
	int version, i;
	char *url;
	struct staged_repo *repo_list = NULL, *repo = NULL;
	char result_file[PATH_MAX];

	/* TODO: fix this once mhaggerts lock changes are in */
	strcpy(result_file, lock->filename.buf);
	i = strlen(result_file) - 5; /* .lock */
	result_file[i] = 0;

	if ((f = fopen(result_file, "r")) == NULL) {
		fprintf(stderr, "Nothing to destage\n");
		goto failed;
	}

	if (!fgets(line, sizeof(line), f)) {
		fprintf(stderr, "Nothing to destage\n");
		goto failed;
	}

	while (line[strlen(line) - 1] == '\n')
		line[strlen(line) - 1] = 0;

	if (!starts_with(line, "version "))
		die(".gitstaging file does not start with a version line.");

	version = strtol(line + 8, &url, 10);
	while (*url == ' ')
		url++;

	if (version > GITSTAGING_VERSION)
		die(".gitstaging file is version %d. This version of git only "
		    "supports version %d or earlier.", version,
		    GITSTAGING_VERSION);

	while (fgets(line, sizeof(line), f)) {
		unsigned char old_sha1[20], new_sha1[20];

		while (line[strlen(line) - 1] == '\n')
			line[strlen(line) - 1] = 0;
		if (starts_with(line, "repo ")) {
			repo = add_staged_repo(&repo_list, line +5);
			continue;
		}
		if (strlen(line) < 83 ||
		    line[40] != ' ' ||
		    line[81] != ' ' ||
		    get_sha1_hex(line, old_sha1) ||
		    get_sha1_hex(line + 41, new_sha1))
			die("Corrupted refs line in staging file: %s", line);

		add_staged_ref(repo, line + 82, old_sha1, new_sha1);
	}

	if (f)
		fclose(f);
	return repo_list;

 failed:
	if (f)
		fclose(f);
	free_staged_repo_list(repo_list);
	return NULL;
}

struct staged_repo *remove_staged_repo(struct staged_repo *repo_list,
				       const char *url)
{
	struct staged_repo *repo;

	/* no repo specified means unstage everything */
	if (!url) {
		free_staged_repo_list(repo_list);
		return NULL;
	}
	if (!repo_list)
		return NULL;

	if (!strcmp(repo_list->url, url)) {
		repo = repo_list->next;
		free_staged_repo(repo_list);
		return repo;
	}

	for(repo = repo_list; repo->next; repo = repo->next) {
		struct staged_repo *tmp = repo->next->next;
		if (!strcmp(repo->next->url, url)) {
			free_staged_repo(repo->next);
			repo->next = tmp;
			return repo_list;
		}
	}
	return repo_list;
}

int write_staging_file(struct lock_file *lock, struct staged_repo *repo_list)
{
	struct strbuf str = STRBUF_INIT;
	if (!repo_list)
		return commit_lock_file(lock);

	strbuf_addf(&str, "version %d %s\n", GITSTAGING_VERSION,
		    repo_list->url);
	while (repo_list) {
		struct staged_ref *ref = repo_list->refs;

		if (!ref) {
			repo_list = repo_list->next;
			continue;
		}

		strbuf_addf(&str, "repo %s\n", repo_list->url);
		while (ref) {
			strbuf_addf(&str, "%s %s %s\n",
				    sha1_to_hex(ref->old_sha1),
				    sha1_to_hex(ref->new_sha1),
				    ref->name);
			ref = ref->next;
		}
		repo_list = repo_list->next;
	}
	if (write_in_full(lock->fd, str.buf, str.len) != str.len)
		die("Failed to write to .gitstaging lock file");

	return commit_lock_file(lock);
}

int remove_staged_ref(struct staged_repo *repo_list, const char *url, const char *refname)
{
	struct staged_repo *repo;
	struct staged_ref *ref, *tmp;

	for(repo = repo_list; repo->next; repo = repo->next)
		if (!strcmp(repo->url, url))
			break;
	if (!repo)
		return -1;

	if (!strcmp(repo->refs->name, refname)) {
		ref = repo->refs;
		repo->refs = ref->next;
		free_staged_ref(ref);
		return 0;
	}
	for (ref = repo->refs; ref->next; ref = ref->next)
		if (!strcmp(ref->next->name, refname))
			break;
	if (!ref->next)
		return -1;
	tmp = ref->next;
	ref->next = tmp->next;
	free_staged_ref(tmp);
	return 0;
}

int lock_staging_file_for_update(struct lock_file *lock)
{
	char *staging;

	staging = expand_user_path("~/.gitstaging");
	if (hold_lock_file_for_update(lock, staging,
				      LOCK_DIE_ON_ERROR|LOCK_NO_DEREF) < 0) {
		fprintf(stderr, "Failed to lock .gitstaging file. %s\n",
			strerror(errno));
		free(staging);
		return -1;
	}
	free(staging);
	return 0;
}
