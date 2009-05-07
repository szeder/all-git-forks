#include "run_git.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "run-command.h"

int
run_git(int argc, const char** argv, struct strbuf* output)
{
    struct child_process proc;
    int i;
    int additional_argc;
    int new_argc;
    char** new_argv;
    int amt;
    int result;
    char buf[2048];

    additional_argc = 1;
    new_argc = argc + additional_argc;
    new_argv = malloc((1+new_argc)*sizeof(char*));
    new_argv[0] = strdup("git");
    for (i=0; i<argc; i++) {
        new_argv[additional_argc + i] = strdup(argv[i]);
    }
    new_argv[new_argc] = NULL;

    if (0) {
        printf("running: ");
        for (i=0; i<new_argc; i++) {
            printf("\"%s\" ", new_argv[i]);
        }
        printf("\n");
    }

    memset(&proc, 0, sizeof(proc));
    proc.argv = (const char**)new_argv;
    proc.in = 0;
    proc.out = -1;
    proc.err = 0;
    proc.git_cmd = 0;

    result = start_command(&proc);
    if (result != 0) {
        return result;
    }

    while ((amt = read(proc.out, buf, sizeof(buf))) > 0) {
        strbuf_add(output, buf, amt);
    }

    result = finish_command(&proc);

    for (i=0; i<new_argc; i++) {
        free(new_argv[i]);
    }
    free(new_argv);

    return result;
}

