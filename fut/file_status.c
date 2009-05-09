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

