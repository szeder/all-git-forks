#ifndef REBASE_INTERACTIVE_H
#define REBASE_INTERACTIVE_H
#include "rebase-common.h"
#include "rebase-todo.h"
#include "oid-array.h"

struct string_list;

const char *git_path_rebase_interactive_dir(void);

struct rebase_interactive {
    struct rebase_options opts;
    char *dir;

    char *todo_file;
    struct rebase_todo_list todo;
    unsigned int todo_offset;
    unsigned int todo_count;

    char *done_file;
    char *msgnum_file, *end_file;
    unsigned int done_count;

    char *author_file;
    char *amend_file;
    char *msg_file;

    char *squash_msg_file;
    char *fixup_msg_file;

    int preserve_merges;
    char *rewritten_dir;
    char *dropped_dir;
    struct oid_array current_commit;

    int autosquash;

    char *rewritten_list_file;
    char *rewritten_pending_file;
    char *stopped_sha_file;
    struct oid_array rewritten_pending;

    struct object_id squash_onto;

    const char *instruction_format;
};

void rebase_interactive_init(struct rebase_interactive *, const char *);

void rebase_interactive_release(struct rebase_interactive *);

int rebase_interactive_in_progress(const struct rebase_interactive *);

int rebase_interactive_load(struct rebase_interactive *);

void rebase_interactive_run(struct rebase_interactive *, const struct string_list *);

void rebase_interactive_continue(struct rebase_interactive *);

void rebase_interactive_skip(struct rebase_interactive *);

#endif /* REBASE_INTERACTIVE_H */
