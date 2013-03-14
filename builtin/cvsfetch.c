#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "builtin.h"
#include "parse-options.h"
#include "vcs-cvs/meta.h"
#include "vcs-cvs/client.h"

/*
Usage: git cvsfetch [options] [refspec]
   options:
        --dry-run -S            - don't update branches, just show new patchsets
        --verbose -v            - be verbose
        --fuzz=<sec> -z         - set time fuzz for patch aggregator (5 min default)
        --nocache               - don't use cached rlog (used by default if its
                                  not older than 5 minutes)
        --cache                 - use cached rlog even if its old

        --authors=<file> -A     - author converion file (.git/cvs-authors by default)

   ODT integration [used to resolve cvs authors and get ticket info]:
       --odt-user -u            - ODT userid (saved to config on first use)
       --odt-password -p        - ODT password (saved to config on first use) [FIXME:]
       --annotate               - annotate branches with ODT ticket title and URL
       --annotate-only          - annotate branches with ODT ticket title and URL and exit

   debug option:
        -Z                      - debug patch aggregator, shows bunch of data
        --rlog=<file>           - use specified rlog file

   refspecs:
        HEAD                    - fetch HEAD into [refs/remotes/]cvs/HEAD

    #
    #   BRANCH2:second_branch   - fetch BRANCH2 into [refs/remotes/]cvs/second_branch
    #   MYBR:my/br              - fetch MYBR into [refs/remotes/]my/br
    #
    # > git cvsfetch --help
    #
    # --dry-run - show patchset, no actual update
    #
    # update
    # > git cvsfetch [refspecs]
    #
    # import
    # > git cvsfetch [--import-date=<date>] refspecs

    # config format
    # [cvsbranch "name"]
    #       merge = refs/removes/cvs/name
*/

unsigned long fileMemoryLimit = 50*1024*1024; /* 50m */

static const char * const builtin_cvsfetch_usage[] = {
	"git cvsfetch [<options>] [<refspec>...]]",
	"git cvsfetch [<options>] [--import-date=<date>] refspecs:<origin branch>",
	NULL
};

enum {
	CACHE_UNSET = 0,
	CACHE_DEFAULT = 1,
	CACHE_SET = 2
};

static int dry_run, verbosity;
static int fuzz = 300;
static const char *authors = NULL;
static int cache = CACHE_DEFAULT;

static struct option builtin_cvsfetch_options[] = {
	OPT__VERBOSITY(&verbosity),
	OPT_BOOLEAN(0, "dry-run", &dry_run,
		    "dry run (show pending patchset and exit)"),
	OPT_SET_INT('c', "cache", &cache,
		    "use cached results even if cache is old", CACHE_SET),
	OPT_SET_INT(0, "nocache", &cache,
		    "don't use cache", CACHE_UNSET),
	OPT_STRING(0, "authors", &authors, NULL,
		    "author converion file (saved as git notes)"),
	OPT_INTEGER('z', "fuzz", &fuzz,
		    "set time fuzz for patch aggregator (default is 5 min)"),
	OPT_END()
};

static int print_revision(void *ptr, void *data)
{
	struct cvs_transport *cvs = data;
	struct file_revision *rev = ptr;
	int rc;

	static struct cvsfile file = CVSFILE_INIT;

	if (rev->prev) {
		struct file_revision *prev = rev->prev;
		while (prev && prev->ismerged && prev->prev)
			prev = prev->prev;
		if (prev->ismerged)
			printf("\tunknown->%s-", rev->prev->revision);
		else
		printf("\t%s->", rev->prev->revision);
	}
	else {
		printf("\tunknown->");
	}
	printf("%s\t%s", rev->revision, rev->path);

	if (rev->isdead)
		printf(" (dead)\n");
	else
		printf("\n");
	rc = cvs_checkout_rev(cvs, rev->path, rev->revision, &file);
	if (rc == -1)
		die("Cannot checkout file %s rev %s", rev->path, rev->revision);
	if (file.isdead) {
		printf("==== file is dead ======\n");
	}
	else {
		printf("==========\n");
		printf("%s", file.file.buf);
		printf("==========\n");
	}
	return 0;
}

void unixtime_to_date(time_t timestamp, struct strbuf *date)
{
	struct tm date_tm;

	strbuf_reset(date);
	strbuf_grow(date, 32);

	setenv("TZ", "UTC", 1);
	tzset();

	memset(&date_tm, 0, sizeof(date_tm));
	gmtime_r(&timestamp, &date_tm);

	date->len = strftime(date->buf, date->alloc, "%Y/%m/%d %T", &date_tm);
}

static struct strbuf sb1 = STRBUF_INIT;
static struct strbuf sb2 = STRBUF_INIT;
static struct strbuf sb3 = STRBUF_INIT;
void print_ps(struct cvs_transport *cvs, struct patchset *ps)
{
	unixtime_to_date(ps->timestamp, &sb1);
	unixtime_to_date(ps->timestamp_last, &sb2);
	unixtime_to_date(ps->cancellation_point, &sb3);

	printf("%s\n"
	       "%s\n"
	       "%s\n"
	       "%s\n"
	       "\n"
	       "%s\n"
	       "\n",
	       ps->author,
	       sb1.buf,
	       sb2.buf,
	       sb3.buf,
	       ps->msg);

	for_each_hash(ps->revision_hash, print_revision, cvs);
}

extern int is_prev_rev(const char *rev1, const char *rev2);
void test(const char *r1, const char *r2)
{
	int rc = is_prev_rev(r1, r2);
	printf("%s -> %s : %d\n", r1, r2, rc);
}

struct refspec
{
	char *cvs_tag;
	char *git_ref;
	char *git_ref_parent; // for import
};

static int called = 0;
void add_file_revision_cb(const char *branch,
			  const char *path,
			  const char *revision,
			  const char *author,
			  const char *msg,
			  time_t timestamp,
			  int isdead,
			  void *data) {
	struct meta_map *branch_meta_map = data;
	struct branch_meta *meta;

	meta = meta_map_find(branch_meta_map, branch);
	if (!meta) {
		meta = new_branch_meta(branch);
		meta_map_add(branch_meta_map, branch, meta);
	}

	add_file_revision(meta, path, revision, author, msg, timestamp, isdead);
	called++;
}

void on_file_checkout(struct cvsfile *file, void *data)
{
	printf("--------------------\n");
	printf("file: %s rev: %s\n", file->path.buf, file->revision.buf);
	printf("mode: %o isdead: %d isbin: %d ismem: %d\n", file->mode, file->isdead, file->isbin, file->ismem);
	if (file->ismem) {
		printf("size: %zu\n", file->file.len);
		printf("-----------\n%s", file->file.buf);
	}
	else {
		printf("temp file name: %s\n", file->file.buf);
	}
	printf("--------------------\n");
}
/*
 * [:method :][[user ][:password ]@]hostname [:[port ]]/path/to/repository
 */

static const char *cvsroot = NULL;
static const char *cvsmodule = NULL;

int git_cvsfetch_config(const char *var, const char *value, void *dummy)
{
	if (!strcmp(var, "cvs.root"))
		return git_config_string(&cvsroot, var, value);
	else if (!strcmp(var, "cvs.module"))
		return git_config_string(&cvsmodule, var, value);
	else if (!strcmp(var, "fileMemoryLimit")) {
		fileMemoryLimit = git_config_ulong(var, value);
		return 0;
	}

	return git_default_config(var, value, dummy);
}

#include "diff.h"
#include "revision.h"
#include "log-tree.h"
#include "builtin.h"
#include "tag.h"
#include "reflog-walk.h"

static int cmd_log_walk(struct rev_info *rev)
{
	struct commit *commit;
	int saved_nrl = 0;
	int saved_dcctc = 0;

	if (prepare_revision_walk(rev))
		die(_("revision walk setup failed"));

	/*
	 * For --check and --exit-code, the exit code is based on CHECK_FAILED
	 * and HAS_CHANGES being accumulated in rev->diffopt, so be careful to
	 * retain that state information if replacing rev->diffopt in this loop
	 */
	while ((commit = get_revision(rev)) != NULL) {
		printf("here\b");
		if (!log_tree_commit(rev, commit) &&
		    rev->max_count >= 0)
			/*
			 * We decremented max_count in get_revision,
			 * but we didn't actually show the commit.
			 */
			rev->max_count++;
		if (!rev->reflog_info) {
			/* we allow cycles in reflog ancestry */
			free(commit->buffer);
			commit->buffer = NULL;
		}
		free_commit_list(commit->parents);
		commit->parents = NULL;
		if (saved_nrl < rev->diffopt.needed_rename_limit)
			saved_nrl = rev->diffopt.needed_rename_limit;
		if (rev->diffopt.degraded_cc_to_c)
			saved_dcctc = 1;
	}
	rev->diffopt.degraded_cc_to_c = saved_dcctc;
	rev->diffopt.needed_rename_limit = saved_nrl;

	if (rev->diffopt.output_format & DIFF_FORMAT_CHECKDIFF &&
	    DIFF_OPT_TST(&rev->diffopt, CHECK_FAILED)) {
		return 02;
	}
	return diff_result_code(&rev->diffopt, 0);
}

void rev_walk(const char *branch_name, const char *prefix)
{
	struct rev_info rev;
	struct setup_revision_opt opt;

	//init_grep_defaults();
	//git_config(git_log_config, NULL);

	init_revisions(&rev, NULL);
	//init_revisions(&rev, prefix);
	//rev.always_show_header = 1;
	//memset(&opt, 0, sizeof(opt));
	//opt.def = "HEAD";
	//opt.revarg_opt = REVARG_COMMITTISH;
	//cmd_log_init(argc, argv, prefix, &rev, &opt);
	printf("walk rc %d\n", cmd_log_walk(&rev));
}

//int main(int argc, const char *argv[])
int cmd_cvsfetch(int argc, const char **argv, const char *prefix)
{
	int ret;
	int i;
	printf("cmd_cvsfetch [prefix='%s']\n", prefix);
	rev_walk("HEAD", prefix);
	exit(0);
	for (i = 0; i < argc; i++) {
		printf(" '%s'", argv[i]);
	}
	printf("\n");


	git_config(git_cvsfetch_config, NULL);

	/*
	 * cvs
	 *	//url = ssh://userid@host.com:/cvs/src/cvs
	 *	root = :ext:dummy@nanobar:37220/home/dummy/SVC/git-cvsfetch
	 *	module = all/src
	 *	fileMemoryLimit = 50m // k, m, or g
	 * cvsbranch "cvs/HEAD"
	 *      tag = HEAD
	 */
	argc = parse_options(argc, argv, prefix, builtin_cvsfetch_options,
			     builtin_cvsfetch_usage, PARSE_OPT_STOP_AT_NON_OPTION);

	for (i = 0; i < argc; i++) {
		printf("ref: '%s'\n", argv[i]);
	}
	//system("env");
	printf("dry-run: %d\n", dry_run);
	printf("cache: %d\n", cache);
	printf("fuzz: %d\n", fuzz);
	printf("cvsroot: %s\n", cvsroot);
	printf("cvsmodule: %s\n", cvsmodule);

	//load_cvs_revision_meta(NULL, "931db7b0fe994ca64daba1d2700085ea5af8d6df", "refs/notes/mine");
	//return 0;

	/*test("1.2.3.8", "1.2.3.9");
	test("1.2.3.9", "1.2.3.8");
	test("1.2.3.9", "1.3.3.10");
	test("1.2.3.8", "1.3.3.9");
	test("1.2.3.9", "1.2.3.10");
	test("1.2.3.9", "1.2.4.0");
	test("1.2.3.9", "1.2.4.9");
	test("3.8", "3.9");
	test("3.9", "3.8");
	test("3.9", "3.10");
	test("4.8", "3.9");
	test("3.9", "3.10");
	test("3.9", "4.0");
	test("3.9", "4.9");
	test("9.9", "10.0");
	test("1.2.9.9", "1.2.10.0");
	test("1.2.9.9", "2.2.10.0");
	test("2.2.9.9", "1.2.10.0");*/

//	struct branch_meta *meta = new_branch_meta("HEAD");

	/*static char *files[] = { "dir/boo.txt", "init", "book.txt" };
	static char *revs[] = { "1.2.1.", "1.", "2." };
	int revsn[] = { 11, 1, 0 };
	static char *authors[] = { "dummy", "kk" };
	static char *msgs[] = { "new task", "", "kk", "new task2" };
	int time = 113;
	int i;
	for (i = time; i < time + 1000000; i++) {
		int file = rand() % 3;
		int author = rand() % 2;
		int msg = rand() % 2;

		char buff[256];
		sprintf(buff, "%s%d", revs[file], revsn[file]++);

		add_file_revision(meta, files[file], buff, authors[author], msgs[msg], i, 0);
	}
	printf("GEN DONE\n");
	*/

//	add_file_revision(meta, "dir/boo.txt", "1.2.1.11", "dummy", "new task", 113, 0);
//	add_file_revision(meta, "init", "1.1", "dummy", "new task", 115, 0);
//	add_file_revision(meta, "book.txt", "1.12", "karl", "kk", 114, 0);
//	add_file_revision(meta, "book.txt", "1.13", "dummy", "new task", 117, 0);
//	add_file_revision(meta, "book.txt", "2.0", "karl", "kk", 118, 0);
//	add_file_revision(meta, "init", "1.2", "karl", "kk", 123, 0);
//	add_file_revision(meta, "dir/boo.txt", "1.2.1.12", "dummy", "new task2", 167, 1);
//
//	aggregate_patchsets(meta);
//
//	struct patchset *ps = meta->patchset_list->head;
//	while (ps) {
//		printf("%s\n"
//		       "%ld\n"
//		       "%ld\n"
//		       "%ld\n"
//		       "\n"
//		       "%s\n"
//		       "\n",
//		       ps->author,
//		       ps->timestamp,
//		       ps->timestamp_last,
//		       ps->cancellation_point,
//		       ps->msg);
//
//		for_each_hash(ps->revision_hash, print_revision, NULL);
//		printf("--------------------\n\n");
//		ps = ps->next;
//	}
//
//	save_cvs_revision_meta(meta, "2950d29062a8e0defee7186806b41fa98ca48bd7", "refs/notes/mine");
//	free_branch_meta(meta);
//

	struct cvs_transport *cvs;

	if (!cvsroot)
		die(_("Please set cvs.root first."));

	cvs = cvs_connect(cvsroot, cvsmodule);
	if (!cvs)
		return -1;

	printf("connected to cvs server\n");

	//char **tags = cvs_gettags(cvs);
	//if  (!tags)
	//	die("cvs_gettags failed");

	//char **p = tags;
	//while (*p) {
	//	printf("tag '%s'\n", *p);
	//	free(*p);
	//}
	//free(tags);

	struct meta_map branch_meta_map;
	meta_map_init(&branch_meta_map);

	cvs_rlog(cvs, 0, 0, add_file_revision_cb, &branch_meta_map);
	printf("CB CALLED: %d\n", called);

	//meta_map_add(&branch_meta_map, "HEAD", new_branch_meta("HEAD"));
	//meta_map_add(&branch_meta_map, "HEAD2", new_branch_meta("HEAD2"));

	struct meta_map_entry *branch_meta;
	int psnum;

	for_each_branch_meta(branch_meta, &branch_meta_map) {
		printf("Working on %s\n", branch_meta->branch_name);
		aggregate_patchsets(branch_meta->meta);

		psnum = 0;
		struct patchset *ps = branch_meta->meta->patchset_list->head;
		while (ps) {
			psnum++;
			print_ps(cvs, ps);
			printf("--------------------\n\n");
			ps = ps->next;
		}
		printf("Branch: %s Commits number: %d\n", branch_meta->branch_name, psnum);
		//break;
	}

	//printf("Find %p\n", meta_map_find(&branch_meta_map, "HEAD2"));

	meta_map_release(&branch_meta_map);

//	struct cvsfile content = CVSFILE_INIT;
//	cvs_checkout_rev(cvs, "b", "1.12", &content);

//	ret = cvs_status(cvs, "b", "1.12", NULL);
//	printf("cvs_status rc=%d\n", ret);
//	ret = cvs_status(cvs, "dir/c", "1.8", NULL);
//	printf("cvs_status rc=%d\n", ret);

//	ret = cvs_checkout_branch(cvs, "mybranch", 0, on_file_checkout, NULL);
//	printf("cvs_checkout_branch rc=%d\n", ret);

	ret = cvs_terminate(cvs);

	printf("done, rc=%d\n", ret);
	return 0;
}
