#include "command.h"

#include <stdio.h>
#include <string.h>

static int
run_unstage(int argc, char const* const* argv)
{
    int i;
    printf("unstaging files:\n");
    for (i=0; i<argc; i++) {
        printf("  %s\n", argv[i]);
    }
    return 0;
}

command_t cmd_unstage = {
    .name = "unstage",
    .func = run_unstage,
    .short_help = "removes files from the stage",
    .long_help = "usage: fut unstage FILES...\n"
                 "\n"
                 "Removes FILES from the stage.  After running this command, FILES\n"
                 "will not be committed if fut commit is run.\n"
};


