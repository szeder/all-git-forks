#include "command.h"

#include <stdio.h>
#include <string.h>

extern command_t cmd_commit;
extern command_t cmd_help;
extern command_t cmd_stage;
extern command_t cmd_status;
extern command_t cmd_unstage;
extern command_t cmd_version;

command_t const* COMMANDS[] = {
    &cmd_commit,
    &cmd_help,
    &cmd_stage,
    &cmd_status,
    &cmd_unstage,
    &cmd_version,
    NULL
};

int main(int argc, char const** argv)
{
    command_t const** p;

    if (argc <= 1) {
        return cmd_help.func(0, NULL);
    }

    // dispatch command
    argc--;
    argv++;
    p = COMMANDS;
    while (*p) {
        if (0 == strcmp((*p)->name, *argv)) {
            argc--;
            argv++;
            return (*p)->func(argc, argv);
        }
        p++;
    }

    return 0;
}
