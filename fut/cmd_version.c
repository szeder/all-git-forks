#include "command.h"

#include <stdio.h>
#include <string.h>

const char * const FUT_VERSION = "0.0";

static int
run_version(int argc, char const * const * argv)
{
    printf("Friendly User Tool version %s\n", FUT_VERSION);
    return 0;
}

command_t cmd_version = {
    .name = "version",
    .func = run_version,
    .short_help = "prints the version of fut",
    .long_help = "usage: fut version -- print the version of fut\n"
};

