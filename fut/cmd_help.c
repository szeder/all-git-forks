#include "command.h"

#include <stdio.h>
#include <string.h>

extern command_t const* COMMANDS[];

static void
help_summary()
{
    command_t const** p;
    int maxlen;
    char format[20];

    maxlen = 0;
    p = COMMANDS;
    while (*p) {
        int len;
        len = strlen((*p)->name);
        if (len > maxlen) {
            maxlen = len;
        }
        p++;
    }
    sprintf(format, "  %%-%ds   %%s\n", maxlen);

    printf("usage: fut COMMAND ARGS\n");
    printf("\n");
    printf("Commands:\n");
    p = COMMANDS;
    while (*p) {
        printf(format, (*p)->name, (*p)->short_help);
        p++;
    }
}

static void
help_detailed(char const* command)
{
    command_t const** p;

    p = COMMANDS;
    while (*p) {
        if (0 == strcmp(command, (*p)->name)) {
            printf("%s", (*p)->long_help);
            return;
        }
        p++;
    }

    printf("unknown command: %s\n", command);
    printf("run fut help for a list of commands\n");
}

static int
run_help(int argc, char const* const* argv)
{
    if (argc == 1) {
        help_detailed(argv[0]);
    } else {
        help_summary();
    }
    return 0;
}

command_t cmd_help = {
    .name = "help",
    .func = run_help,
    .short_help = "provides help for the fut commands",
    .long_help = "usage: fut help -- list all commands\n"
                 "usage: fut help COMMAND -- get help on COMMAND\n"
};

