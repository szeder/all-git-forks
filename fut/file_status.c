#include "file_status.h"

#include "cache.h"
#include "diff.h"
#include "commit.h"
#include "revision.h"
#include "builtin.h"
#include "diffcore.h"
#include "dir.h"

file_status_vector_t* g_files;

char const* DIFF_WORKING_DIR_ARGS[] = {
        "diff-files",
        "--name-status"
    };

void
init_file_status_vector(file_status_vector_t* v)
{
    int bufsize;
    v->count = 0;
    v->alloc = 20;
    bufsize = sizeof(file_status_t)*v->alloc;
    v->files = malloc(bufsize);
    memset(v->files, 0xab, bufsize);
}

void
destroy_file_status_vector(file_status_vector_t* v)
{
    int i;
    const int N = v->count;
    for (i=0; i<N; i++) {
        strbuf_release(&(v->files[i].filename));
    }
    free(v->files);
    v->files = NULL;
    v->count = 0xdeadbeef;
    v->alloc = 0xdeadbeef;
}

// makes a copy of status
void 
insert_file_status(file_status_vector_t* v, char status, const char* path)
{
    int i;
    int cmp;
    int N = v->count;
    int len = strlen(path);
    int bufsize;
    for (i=0; i<v->count; i++) {
        cmp = strcmp(path, v->files[i].filename.buf);
        if (cmp < 0) {
            break;
        }
    }
    if ((N+1) < v->alloc) {
        v->alloc *= 2;
        bufsize = sizeof(file_status_t)*v->alloc;
        file_status_t* tmp = malloc(bufsize);
        memset(tmp, 0xab, bufsize);
        if (i != 0) {
            memcpy(tmp, v->files, sizeof(file_status_t)*i);
        }
        if (i != N) {
            memcpy(tmp+i+1, v->files+i, sizeof(file_status_t)*(N-i));
        }
        free(v->files);
        v->files = tmp;
    } else {
        if (i != N) {
            memmove(v->files+i+1, v->files+i, sizeof(file_status_t)*N-i);
        }
    }
    strbuf_init(&v->files[i].filename, len);
    strbuf_add(&v->files[i].filename, path, len);
    v->files[i].status = status;
    v->count++;
}

void
delete_file_status(file_status_vector_t* v, int index)
{
    strbuf_release(&v->files[index].filename);
    if (index != v->count-1) {
        memmove(v->files+index, v->files+index+1, sizeof(file_status_t)*(v->count-index));
    }
    v->count--;
    memset(v->files+v->count, 0xbc, sizeof(file_status_t));
}

void
save_files_callback(struct diff_queue_struct *q, struct diff_options *opt, void *data)
{
    int i;
    for (i=0; i<q->nr; i++) {
        const struct diff_filepair* entry = q->queue[i];
        insert_file_status(g_files, entry->status, entry->one->path);
    }
}

int
get_working_directory_changed_files(file_status_vector_t* v)
{
    int result;
    struct rev_info rev;
    const char* prefix;

    prefix = setup_git_directory();
    setup_work_tree();

    init_revisions(&rev, prefix);

    rev.diffopt.output_format = DIFF_FORMAT_CALLBACK;
    rev.diffopt.format_callback = save_files_callback;
    
    if (read_cache_preload(rev.diffopt.paths) < 0) {
        perror("read_cache_preload");
        return -1;
    }
    
    g_files = v;
    result = run_diff_files(&rev, 0);
    g_files = NULL;
    return diff_result_code(&rev.diffopt, result);
}

int
get_staged_files(file_status_vector_t* v)
{
    int result;
    struct rev_info rev;
    const char* prefix;

    prefix = setup_git_directory();
    setup_work_tree();

    init_revisions(&rev, prefix);
    setup_revisions(0, NULL, &rev, "HEAD");
//        s->is_initial ? EMPTY_TREE_SHA1_HEX : s->reference);
    
    rev.diffopt.output_format = DIFF_FORMAT_CALLBACK;
    rev.diffopt.format_callback = save_files_callback;
    
    if (read_cache_preload(rev.diffopt.paths) < 0) {
        perror("read_cache_preload");
        return -1;
    }

    g_files = v;
    result = run_diff_index(&rev, 1);
    g_files = NULL;
    // putting 0 here makes it find all of the working directory changes
    // as well.
    return diff_result_code(&rev.diffopt, result);
}

int
get_untracked_files(file_status_vector_t* v)
{
    struct dir_struct dir;
    int i;
    struct strbuf buf = STRBUF_INIT;

    memset(&dir, 0, sizeof(dir));

    /* TODO: Do we want to show all files (i.e. checktree) or just the directories?
    if (!s->untracked)
        dir.flags |=
            DIR_SHOW_OTHER_DIRECTORIES | DIR_HIDE_EMPTY_DIRECTORIES;
    */
    setup_standard_excludes(&dir);

    read_directory(&dir, ".", "", 0, NULL);
    for(i = 0; i < dir.nr; i++) {
        struct dir_entry *ent = dir.entries[i];
        if (!cache_name_is_other(ent->name, ent->len))
            continue;
        insert_file_status(v, 'K', ent->name);
    }
    strbuf_release(&buf);
    return 0;
}

const char*
status_to_status_label(char status)
{
    switch (status) {
        case 'A':
        case 'K':
            return "added:      ";
        case 'C':
            return "copied:     ";
        case 'D':
            return "deleted:    ";
        case 'M':
            return "modified:   ";
        case 'R':
            return "renamed:    ";
        case 'T':
            return "modechanged:";
        case 'U':
            return "unmerged:   ";
        case 'X':
        default:
            return "unknown:    ";
    }
}

char
combine_statuses(char staged, char working)
{
    if (working == 'D') {
        // If it's deleted in the the working dir, who cares what's in the stage,
        // unless it was added, in which case, we just won't even show it.
        if (staged == 'A') {
            return ' ';
        } else {
            return 'D';
        }
    }
    if (staged == 'A') {
        return 'A';
    }
    return working;
}

int
write_file_status(FILE* out)
{
    int i, j, k;
    int err;
    file_status_vector_t working;
    file_status_vector_t staged;
    file_status_vector_t untracked;
    file_status_vector_t files;

    init_file_status_vector(&working);
    init_file_status_vector(&staged);
    init_file_status_vector(&untracked);
    init_file_status_vector(&files);

    err = get_working_directory_changed_files(&working);
    err = get_staged_files(&staged);
    err = get_untracked_files(&untracked);

    // Combine the files that are in both working and staged.
    // Generally we show the working status, because it's going
    // to be the final one, except in the cases where the changes
    // in the working directory aren't interesting, like a
    // modification on top of an add.
    i=0;
    j=0;
    while (i<working.count && j<staged.count) {
        const file_status_t* p = working.files + i;
        const file_status_t* q = staged.files + j;
        int cmp = strbuf_cmp(&p->filename, &q->filename);
        if (0 == cmp) { // equal
            char combined = combine_statuses(q->status, p->status);
            if (combined != ' ') {
                insert_file_status(&files, combined, p->filename.buf);
            }
            i++;
            j++;
        }
        else if (cmp < 0) {
            insert_file_status(&files, p->status, p->filename.buf);
            i++;
        }
        else { // cmp > 0
            insert_file_status(&files, q->status, q->filename.buf);
            j++;
        }
    }
    while (i<working.count) {
        const file_status_t* p = working.files + i;
        insert_file_status(&files, p->status, p->filename.buf);
        i++;
    }
    while (j<staged.count) {
        const file_status_t* q = staged.files + j;
        insert_file_status(&files, q->status, q->filename.buf);
        j++;
    }
    for (k=0; k<untracked.count; k++) {
        const file_status_t* r = untracked.files + k;
        insert_file_status(&files, r->status, r->filename.buf);
        k++;
    }

    for (i=0; i<files.count; i++) {
        fprintf(out, "  %s %s\n", status_to_status_label(files.files[i].status),
                files.files[i].filename.buf);
    }

    return 0;
}

