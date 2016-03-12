#ifndef REBASE_INTERACTIVE_H
#define REBASE_INTERACTIVE_H
#include "rebase-common.h"
#include "rebase-todo.h"

const char *git_path_rebase_interactive_dir(void);

struct rebase_interactive {
    struct rebase_options opts;
    char *dir;

    char *todo_file;
    struct rebase_todo_list todo;
    unsigned int todo_offset;
    unsigned int todo_count;

    char *done_file;
    unsigned int done_count;

    const char *instruction_format;
};

void rebase_interactive_init(struct rebase_interactive *, const char *);

void rebase_interactive_release(struct rebase_interactive *);

int rebase_interactive_in_progress(const struct rebase_interactive *);

int rebase_interactive_load(struct rebase_interactive *);

void rebase_interactive_run(struct rebase_interactive *);

#endif /* REBASE_INTERACTIVE_H */
