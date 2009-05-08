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

typedef struct file_status_vector_t
{
    file_status_t* files;
    int count;
    int alloc;
} file_status_vector_t;

// vector management
void init_file_status_vector(file_status_vector_t*);
void destroy_file_status_vector(file_status_vector_t*);
void insert_file_status(file_status_vector_t* v, char status, const char* path);

// scanning the files
int get_working_directory_changed_files(file_status_vector_t* v);
int get_staged_files(file_status_vector_t* v);
int get_untracked_files(file_status_vector_t* v);

// Returns a human-readable status string for one of the status code chars.
const char* status_to_status_label(char status);


#endif FILE_STATUS_H

