#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "command.h"
#include "file_status.h"

extern command_t cmd_stage;

static int option_force = 0;

static int
stage_interactive(void)
{
    int i;
    int err;
    file_status_vector_t files;

    init_file_status_vector(&files);

    printf("stage_interactive option_force=%d\n", option_force);

    err = get_working_directory_changed_files(&files);
    err = get_staged_files(&files);
    err = get_untracked_files(&files);

    for (i=0; i<files.count; i++) {
        printf("  %s (%s)\n", files.files[i].filename.buf,
                status_to_status_label(files.files[i].status));
    }

    return 0;
}

static int
stage_files(int count, char const* const* filenames)
{
    int i;
    printf("staging files option_force=%d:\n", option_force);
    for (i=0; i<count; i++) {
        printf("  %s\n", filenames[i]);
    }
    return 0;
}

static int
run_stage(int argc, char const* const* argv)
{
    int ch = 'z';

    optind = 0;
    while ((ch = getopt(argc, (char* const*) argv, "f")) != -1) {
        switch (ch) {
            case 'f':
                option_force = 1;
                break;
            default:
                fprintf(stderr, "Unknown option: -%c\n", ch);
                fprintf(stderr, cmd_stage.long_help);
                return 1;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc == 0) {
        return stage_interactive();
    } else {
        return stage_files(argc, argv);
    }
}

command_t cmd_stage = {
    .name = "stage",
    .func = run_stage,
    .short_help = "adds files to the stage",
    .long_help = "usage: fut stage [-f]\n"
                 "  Runs an interactive editor to select the files to stage\n"
                 "  from the files that have been modified\n"
                 "\n"
                 "usage: fut stage [-f] FILES...\n"
                 "  Adds FILES to the stage in preparation to be committed.\n"
                 "\n"
                 "OPTIONS\n"
                 "  -f   Forces all files to be staged, ignoring .gitignore.\n"
};

