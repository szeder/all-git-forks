#ifndef FILE_STATUS_H
#define FILE_STATUS_H

#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#include "strbuf.h"

typedef struct file_status_t
{
    struct strbuf filename;
    char status;
} file_status_t;

int get_working_directory_changed_files(file_status_t** files, int* count);
int get_staged_files(file_status_t** files, int* count);
int get_untracked_files(file_status_t** files, int* count);

#endif FILE_STATUS_H

