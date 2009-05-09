#include "command.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "strbuf.h"
#include "file_status.h"

int
run_commit(int argc, char const* const* argv)
{
    int result;
    char tmp_file_path[PATH_MAX];
    struct strbuf buffer;
    int fd;
    FILE* tmp_file;

    printf("commit!\n");

    strcpy(tmp_file_path, "/tmp/commit.XXXXXX.fut");

    fd = mkstemps(tmp_file_path, 4);
    if (fd == -1) {
        fprintf(stderr, "Error opening temp file: %s.\n", tmp_file_path);
        return 1;
    }

    tmp_file = fdopen(fd, "w");

    fprintf(tmp_file, "# Remove any files you don't want to commit, and enter a\n"
            "# commit message.  To cancel, leave an empty commit message.\n"
            "\n");
    fprintf(tmp_file, "Files:\n");
    write_file_status(tmp_file);
    fprintf(tmp_file, "\n");
    fprintf(tmp_file, "Commit Message:\n");
    fclose(tmp_file);

    result = launch_editor(tmp_file_path, NULL, NULL);

    if (result != 0) {
        fprintf(stderr, "result=%d\n", result);
        return 1;
    }

    if (strbuf_read_file(&buffer, tmp_file_path, 0) < 0) {
        result = 1;
        fprintf(stderr, "Error reading temp file. %s\n", strerror(errno));
        goto done;
    }

    printf("They wrote --[%s]--\n", buffer.buf);

done:
    unlink(tmp_file_path);

    return result;
}

command_t cmd_commit = {
    .name = "commit",
    .func = run_commit,
    .short_help = "commits the current stage to the repository",
    .long_help = "usage: fut commit\n"
                 "  Commits the files in the working directory.  Once\n"
                 "  a file has been added to the stage, committing\n"
                 "  with this version of commit will pull any additional\n"
                 "  changes made in that file in the working directory\n"
                 "  into the commit.\n"
                 "\n"
                 "usage: fut commit --stage\n"
                 "  Commits the files as they exist on the stage\n"
                 "  does not look at the working directory.\n"
};

