#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "command.h"
#include "file_status.h"

extern command_t cmd_status;

static int
full_status(void)
{
    int i, j, k;
    int err;
    file_status_vector_t working;
    file_status_vector_t staged;
    file_status_vector_t untracked;
    file_status_vector_t files;

    init_file_status_vector(&working);
    init_file_status_vector(&staged);
    init_file_status_vector(&untracked);
    init_file_status_vector(&files);

    err = get_working_directory_changed_files(&working);
    err = get_staged_files(&staged);
    err = get_untracked_files(&untracked);

    // Combine the files that are in both working and staged.
    // Generally we show the working status, because it's going
    // to be the final one, except in the cases where the changes
    // in the working directory aren't interesting, like a
    // modification on top of an add.
    i=0;
    j=0;
    while (i<working.count && j<staged.count) {
        const file_status_t* p = working.files + i;
        const file_status_t* q = staged.files + j;
        int cmp = strbuf_cmp(&p->filename, &q->filename);
        if (0 == cmp) { // equal
            char combined = combine_statuses(q->status, p->status);
            if (combined != ' ') {
                insert_file_status(&files, combined, p->filename.buf);
            }
            i++;
            j++;
        }
        else if (cmp < 0) {
            insert_file_status(&files, p->status, p->filename.buf);
            i++;
        }
        else { // cmp > 0
            insert_file_status(&files, q->status, q->filename.buf);
            j++;
        }
    }
    while (i<working.count) {
        const file_status_t* p = working.files + i;
        insert_file_status(&files, p->status, p->filename.buf);
        i++;
    }
    while (j<staged.count) {
        const file_status_t* q = staged.files + j;
        insert_file_status(&files, q->status, q->filename.buf);
        j++;
    }
    for (k=0; k<untracked.count; k++) {
        const file_status_t* r = untracked.files + k;
        insert_file_status(&files, r->status, r->filename.buf);
        k++;
    }

    for (i=0; i<files.count; i++) {
        printf("  %s %s\n", status_to_status_label(files.files[i].status),
                files.files[i].filename.buf);
    }

    return 0;
}

static int
run_status(int argc, char const* const* argv)
{
    int ch = 'z';

    optind = 0;
    while ((ch = getopt(argc, (char* const*) argv, "f")) != -1) {
        switch (ch) {
            default:
                fprintf(stderr, "Unknown option: -%c\n", ch);
                fprintf(stderr, cmd_status.long_help);
                return 1;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc == 0) {
        return full_status();
    } else {
        return fprintf(stderr, cmd_status.long_help);
    }
}

command_t cmd_status = {
    .name = "status",
    .func = run_status,
    .short_help = "view the status of files in the working directory and stage",
    .long_help = "usage: fut status\n"
                 "  Prints the status of the files that have been changed in\n"
                 "  some way in the working directory and on the stage.\n"
};
