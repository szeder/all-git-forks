/**
 * "git talk"
 * A dummy command for git which simply prints to the screen
 */
#include <stdio.h>
int cmd_talk(int argc, const char **argv, const char *prefix)
{
    printf("This is the talk command\n");
}
