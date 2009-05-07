#include "file_status.h"

#include "cache.h"
#include "diff.h"
#include "commit.h"
#include "revision.h"
#include "builtin.h"
#include "diffcore.h"

char const* DIFF_WORKING_DIR_ARGS[] = {
        "diff-files",
        "--name-status"
    };

file_status_t** g_files;
int* g_fileCount;
int g_fileAlloc;

void
init_save_files(file_status_t** files, int* count)
{
    g_files = files;
    g_fileCount = count;
    g_fileAlloc = 0;
}


void
save_files_callback(struct diff_queue_struct *q, struct diff_options *opt, void *data)
{
    int i;
    for (i=0; i<q->nr; i++) {
        const struct diff_filepair* entry = q->queue[i];
        fprintf(stderr, "  status=%c file0=%s file1=%s\n", entry->status, entry->one->path,
                entry->two->path);
    }
}

int
get_working_directory_changed_files(file_status_t** files, int* count)
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
    
    fprintf(stderr, "Working directory changes:\n");
    init_save_files(files, count);
	result = run_diff_files(&rev, 0);
	return diff_result_code(&rev.diffopt, result);
}

int
get_staged_files(file_status_t** files, int* count)
{
    int result;
	struct rev_info rev;
    const char* prefix;

    prefix = setup_git_directory();
    setup_work_tree();

	init_revisions(&rev, prefix);
	setup_revisions(0, NULL, &rev, "HEAD");
//		s->is_initial ? EMPTY_TREE_SHA1_HEX : s->reference);
    
    rev.diffopt.output_format = DIFF_FORMAT_CALLBACK;
    rev.diffopt.format_callback = save_files_callback;
    
	if (read_cache_preload(rev.diffopt.paths) < 0) {
		perror("read_cache_preload");
		return -1;
	}

    fprintf(stderr, "Staged changes:\n");
    init_save_files(files, count);
	result = run_diff_index(&rev, 1);
    // putting 0 here makes it find all of the working directory changes
    // as well.
	return diff_result_code(&rev.diffopt, result);
}

/*
int
get_untracked_files(file_status_t** files, int* count)
{
	struct dir_struct dir;
	int i;
	int shown_header = 0;
	struct strbuf buf = STRBUF_INIT;

	memset(&dir, 0, sizeof(dir));

	if (!s->untracked)
		dir.flags |=
			DIR_SHOW_OTHER_DIRECTORIES | DIR_HIDE_EMPTY_DIRECTORIES;
	setup_standard_excludes(&dir);

	read_directory(&dir, ".", "", 0, NULL);
	for(i = 0; i < dir.nr; i++) {
		struct dir_entry *ent = dir.entries[i];
		if (!cache_name_is_other(ent->name, ent->len))
			continue;
		if (!shown_header) {
			s->workdir_untracked = 1;
			wt_status_print_untracked_header(s);
			shown_header = 1;
		}
		color_fprintf(s->fp, color(WT_STATUS_HEADER), "#\t");
		color_fprintf_ln(s->fp, color(WT_STATUS_UNTRACKED), "%s",
				quote_path(ent->name, ent->len,
					&buf, s->prefix));
	}
	strbuf_release(&buf);
}
*/

