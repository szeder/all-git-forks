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
#include "string-list.h"

// returns 0 until it's past the end of the string
// only works on unix newlines ('\n' only)
static int
get_strbuf_line(struct strbuf* str, int* pos, struct strbuf* line)
{
    int begin;
    int len;
    char* buf;
    len = str->len;
    if (*pos >= len) {
        return 1;
    }
    begin = *pos;
    buf = str->buf;
    while (*pos < len && (buf[*pos] != '\r' && buf[*pos] != '\n')) {
        (*pos)++;
    }
    strbuf_splice(line, 0, line->len, str->buf+begin, (*pos)-begin);
    if (buf[*pos] == '\r' && *pos < (len-1) && buf[(*pos)+1] == '\n') {
        (*pos) += 2;
    } else {
        (*pos)++;
    }
    return 0;
}

static int
is_comment(struct strbuf* line)
{
    char* p = line->buf;
    char c;
    int i;
    int len = line->len;
    for (i=0; i<len; i++) {
        c = p[i];
        if (c == ' ' || c == '\t') {
            continue;
        }
        if (c == '#') {
            return 1;
        }
    }
    return 0;
}

static int
strip_file_info(struct strbuf* str)
{
    return 0;
}

static int
parse_commit(struct strbuf* input, struct string_list* files, struct strbuf* message)
{
    int err;
    int pos;
    struct strbuf line;
    int lineno = 0;
    enum { BEGIN, FILES, MESSAGE } state = BEGIN;
    enum { OK, FAILED } result = OK;

    // if there is no data, just return
    if (input->len == 0) {
        return 0;
    }

    strbuf_init(&line, 0);
    pos = 0;

    while (result == OK) {
        err = get_strbuf_line(input, &pos, &line);
        lineno++;
        if (err != 0) {
            break;
        }
        if (is_comment(&line)) {
            continue;
        }
        switch (state) {
            case BEGIN:
                strbuf_trim(&line);
                if (line.len != 0) {
                    if (0 == strcmp(line.buf, "Files:")) {
                        state = FILES;
                    } else {
                        result = FAILED;
                    }
                }
                break;
            case FILES:
                strbuf_trim(&line);
                if (line.len != 0) {
                    if (0 == strcmp(line.buf, "Commit Message:")) {
                        state = MESSAGE;
                    }
                    else if (0 == strip_file_info(&line)) {
                        string_list_insert(line.buf, files);
                    }
                    else {
                        result = FAILED;
                    }
                }
                break;
            case MESSAGE:
                strbuf_rtrim(&line);
                strbuf_addbuf(message, &line);
                strbuf_addch(message, '\n');
                break;
        }
    }
    strbuf_trim(message);
    return result == OK ? 0 : 1;
}

int
run_commit(int argc, char const* const* argv)
{
    int result;
    char tmp_file_path[PATH_MAX];
    struct strbuf buffer;
    struct string_list files;
    struct strbuf message;
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
    fprintf(tmp_file, "Commit Message:\n\n");
    fclose(tmp_file);

    result = launch_editor(tmp_file_path, NULL, NULL);
    printf("editor returned %d\n", result);

    if (result != 0) {
        fprintf(stderr, "result=%d\n", result);
        return 1;
    }

    strbuf_init(&buffer, 0);
    strbuf_init(&message, 0);
    memset(&files, 0, sizeof(struct string_list));
    files.strdup_strings = 1;

    if (strbuf_read_file(&buffer, tmp_file_path, 0) < 0) {
        result = 1;
        fprintf(stderr, "Error reading temp file. %s\n", strerror(errno));
        goto done;
    }

    printf("They wrote --[%s]--\n", buffer.buf);

    result = parse_commit(&buffer, &files, &message);
    if (result != 0) {
        fprintf(stderr, "Error parsing commit message.\n");
        result = 1;
        goto done;
    }

    if (message.len == 0 || files.nr == 0) {
        fprintf(stderr, "Commit aborted.\n");
        result = 1;
        goto done;
    }
    
    printf("file count %d\n", files.nr);
    print_string_list("files--", &files);
    printf("\nMessage --[[%s]]--\n", message.buf);

done:
    strbuf_release(&buffer);
    strbuf_release(&message);
    string_list_clear(&files, 0);
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

