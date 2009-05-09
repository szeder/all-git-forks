#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "command.h"
#include "file_status.h"

extern command_t cmd_status;

static int
full_status(void)
{
    return write_file_status(stdout);
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
