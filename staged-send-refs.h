#ifndef _STAGED_SEND_REFS_H_
#define _STAGED_SEND_REFS_H_

#define GITSTAGING_VERSION 0

struct staged_ref {
	struct staged_ref *next;
	char *name;
	unsigned char old_sha1[20], new_sha1[20];
	char *error_str;
};

struct staged_repo {
	struct staged_repo *next;
	struct staged_ref *refs;
	char *url;
	char *error_str;
};

void free_staged_repo(struct staged_repo *item);
void free_staged_ref(struct staged_ref *ref);
void free_staged_repo_list(struct staged_repo *list);
struct staged_repo *add_staged_repo(struct staged_repo **repo_list, const char *url);
struct staged_repo *remove_staged_repo(struct staged_repo *repo_list, const char *url);
int add_staged_ref(struct staged_repo *repo, const char *name, const unsigned char *old_sha1, const unsigned char *new_sha1);
int remove_staged_ref(struct staged_repo *repo_list, const char *url, const char *refname);
struct staged_repo *read_staging_file(struct lock_file *lock);
int write_staging_file(struct lock_file *lock, struct staged_repo *repo_list);
int lock_staging_file_for_update(struct lock_file *lock);

#endif
