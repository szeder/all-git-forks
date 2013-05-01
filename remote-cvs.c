#include "cache.h"
#include "remote.h"
#include "strbuf.h"
#include "url.h"
#include "refs.h"
#include "exec_cmd.h"
#include "run-command.h"
#include "vcs-cvs/cvs-client.h"
#include "vcs-cvs/meta-store.h"
#include "vcs-cvs/aggregator.h"
#include "vcs-cvs/proto-trace.h"
#include "vcs-cvs/ident-utils.h"
#include "notes.h"
#include "argv-array.h"
#include "commit.h"
#include "progress.h"
#include "diff.h"
#include "string-list.h"

/*
 * FIXME:
 * - import of new branch may fail, if parent branch ref is not created yet
 *   (fast-import haven't created it yet)
 *
 * TODO:
 * -/+ skip dead file addition to HEAD branch
 * - depth
 * - check that metadata correspond to ls-tree files (all files have rev, but no extra)
 * - authors ref/note
 * - safe cancelation point + update time for branch OR ref cmp
 * - delay cvs connect until needed
 * - support options (progress, verbosity, dry-run)
 * - save CVS error and info messages in buffer
 * - sort rlog, avoid extra commits splits done same seconds
 * - validation code: rls -R -d -e -D 'Apr 27 12:37:19 2013'
 *
 * KNOWN PITFALLS:
 * - CVS has not symlinks support
 * - CVS file permittions history is not tracked (CVS have that feature commented)
 * - CVS file permittions cannot be changed for existing files
 *
 * [cvshelper]
 *	ignoreModeChange - bool
 *	fileMemoryLimit - long
 *	pushNoRefsUpdate - bool
 *	verifyImport - bool
 *	cvsProtoTrace - path
 *	remoteHelperTrace - path
 */

static const char trace_key[] = "GIT_TRACE_CVS_HELPER";
static const char trace_proto[] = "RHELPER";
static const char dump_cvs_commit[] = "GIT_DUMP_PATCHSETS";
/*
 * FIXME:
 */
unsigned long fileMemoryLimit = 2 * 1024 * 1024 * 1024L; //50*1024*1024; /* 50m */

static int depth = 0;
static int verbosity = 0;
static int progress = 0;
static int followtags = 0;
static int dry_run = 0;
static int initial_import = 0;

static int no_refs_update_on_push = 0;
static int ignore_mode_change = 0;
static int verify_import = 0;
//static struct progress *progress_state;
//static struct progress *progress_rlog;

static int revisions_all_branches_total = 0;
static int revisions_all_branches_fetched = 0;
static int skipped = 0;
static time_t import_start_time = 0;
//static off_t fetched_total_size = 0;

static const char *cvsmodule = NULL;
static const char *cvsroot = NULL;
static struct cvs_transport *cvs = NULL;
static struct strbuf push_error_sb = STRBUF_INIT;
static struct string_list cvs_branch_list = STRING_LIST_INIT_DUP;
static struct string_list *import_branch_list = NULL;

static const char import_commit_edit[] = "IMPORT_COMMIT_EDIT";
static const char export_commit_edit[] = "EXPORT_COMMIT_EDIT";
static int have_import_hook = 0;
static int have_export_hook = 0;

static int cmd_capabilities(const char *line);
static int cmd_option(const char *line);
static int cmd_list(const char *line);
static int cmd_list_for_push(const char *line);
static int cmd_batch_import(struct string_list *list);
static int cmd_batch_push(struct string_list *list);
static int import_branch_by_name(const char *branch_name);
static int validate_commit_meta_by_tree(const char *ref, struct hash_table *revision_meta_hash);

typedef int (*input_command_handler)(const char *);
typedef int (*input_batch_command_handler)(struct string_list *);
struct input_command_entry {
	const char *name;
	input_command_handler fn;
	input_batch_command_handler batch_fn;
	unsigned char batchable;	/* whether the command starts or is part of a batch */
};

static const struct input_command_entry input_command_list[] = {
	{ "capabilities", cmd_capabilities, NULL, 0 },
	{ "option", cmd_option, NULL, 0 },
	{ "import", NULL, cmd_batch_import, 1 },
	{ "bidi-import", NULL, NULL, 0 },
	{ "push", NULL, cmd_batch_push, 1 },
	// `list for-push` should go before `list`, or later would always be run
	{ "list for-push", cmd_list_for_push, NULL, 0 },
	{ "list", cmd_list, NULL, 0 },
	{ NULL, NULL, NULL, 0 }
};

static ssize_t helper_printf(const char *fmt, ...) __attribute__((format (printf, 1, 2)));

static ssize_t helper_printf(const char *fmt, ...)
{
	va_list args;
	ssize_t written;

	va_start(args, fmt);
	written = vprintf(fmt, args);
	va_end(args);

	if (trace_want(trace_key)) {
		struct strbuf tracebuf = STRBUF_INIT;
		va_start(args, fmt);
		strbuf_vaddf(&tracebuf, fmt, args);
		va_end(args);
		proto_trace(tracebuf.buf, tracebuf.len, OUT);
		strbuf_release(&tracebuf);
	}


	return written;
}

static ssize_t helper_write(const char *buf, size_t len)
{
	ssize_t written;
	proto_trace(buf, len, OUT_BLOB);

	written = fwrite(buf, 1, len, stdout);

	return written;
}

static void helper_flush()
{
	fflush(stdout);
	if (trace_want(trace_key)) {
		struct strbuf out = STRBUF_INIT;
		strbuf_addf(&out, "RHELPER      -- FLUSH -->\n");
		trace_strbuf(trace_key, &out);
		strbuf_release(&out);
	}
}

static int helper_strbuf_getline(struct strbuf *sb, FILE *fp, int term)
{
	if (strbuf_getwholeline(sb, fp, term))
		return EOF;

	proto_trace(sb->buf, sb->len, IN);

	if (sb->buf[sb->len-1] == term)
		strbuf_setlen(sb, sb->len-1);

	return 0;
}

static const char *gettext_after(const char *str, const char *what)
{
	size_t len = strlen(what);

	if (!strncmp(str, what, len)) {
		return str + len;
	}
	return NULL;
}

static int cmd_capabilities(const char *line)
{
	helper_printf("import\n");
	helper_printf("bidi-import\n");
	helper_printf("push\n");
	helper_printf("option\n");
	helper_printf("refspec refs/heads/*:%s*\n", get_private_ref_prefix());
	helper_printf("\n");
	//helper_printf("refspec %s:%s\n\n", remote_ref, private_ref);
	helper_flush();
	return 0;
}

static int cmd_option(const char *line)
{
	const char *opt;
	const char *val;

	opt = gettext_after(line, "option ");
	if (!opt)
		die("Malformed option request");

	if ((val = gettext_after(opt, "depth "))) {
		depth = atoi(val);
	}
	else if ((val = gettext_after(opt, "verbosity "))) {
		verbosity = atoi(val);
	}
	else if ((val = gettext_after(opt, "progress "))) {
		if (strcmp(val, "true"))
			progress = 1;
	}
	else if ((val = gettext_after(opt, "dry-run "))) {
		if (strcmp(val, "true"))
			dry_run = 1;
	}
	else if ((val = gettext_after(opt, "followtags "))) {
		if (strcmp(val, "true"))
			followtags = 1;
	}
	else {
		helper_printf("unsupported\n");
		helper_flush();
		return 0;
	}
	helper_printf("ok\n");
	helper_flush();
	return 0;
}

static const char *get_import_time_estimation()
{
	static const char *now = " 00:00, ";
	static struct strbuf eta_sb = STRBUF_INIT;
	time_t eta = time(NULL) - import_start_time;
	struct tm tm_eta;

	if (!eta)
		return now;

	eta = (double)eta / (double)revisions_all_branches_fetched *
		(double)(revisions_all_branches_total - revisions_all_branches_fetched);

	if (!eta)
		return now;

	memset(&tm_eta, 0, sizeof(tm_eta));
	gmtime_r(&eta, &tm_eta);

	strbuf_reset(&eta_sb);
	if (tm_eta.tm_year - 70) // year since 1900, 1970 is unix time start
		strbuf_addf(&eta_sb, " %d years, ", tm_eta.tm_year - 70);
	if (tm_eta.tm_mon)
		strbuf_addf(&eta_sb, " %d months, ", tm_eta.tm_mon);
	if (tm_eta.tm_mday - 1)
		strbuf_addf(&eta_sb, " %d days, ", tm_eta.tm_mday - 1);
	if (tm_eta.tm_hour)
		strbuf_addf(&eta_sb, " %d hours, ", tm_eta.tm_hour);
	if (tm_eta.tm_min)
		strbuf_addf(&eta_sb, " %d min, ", tm_eta.tm_min);
	if (tm_eta.tm_sec)
		strbuf_addf(&eta_sb, " %d sec, ", tm_eta.tm_sec);

	return eta_sb.buf;
}

static int fast_export_revision_cb(void *ptr, void *data)
{
	static struct cvsfile file = CVSFILE_INIT;
	struct cvs_revision *rev = ptr;
	int rc;

	revisions_all_branches_fetched++;
	fprintf(stderr, "checkout [%d/%d] (%.2lf%%) all branches,%sETA] %s %s",
			revisions_all_branches_fetched,
			revisions_all_branches_total,
			(double)revisions_all_branches_fetched/(double)revisions_all_branches_total*100.,
			get_import_time_estimation(),
			rev->path, rev->revision);

	if (rev->isdead) {
		fprintf(stderr, " dead\n");
		helper_printf("D %s\n", rev->path);
		return 0;
	}

	rc = cvs_checkout_rev(cvs, rev->path, rev->revision, &file);
	if (rc == -1)
		die("Cannot checkout file %s rev %s", rev->path, rev->revision);

	//fetched_total_size += file.file.len;
	//display_progress(progress_state, revisions_all_branches_fetched);
	//display_throughput(progress_state, fetched_total_size);

	if (file.isdead) {
		fprintf(stderr, " (fetched) dead\n");
		helper_printf("D %s\n", rev->path);
		return 0;
	}

	if (file.iscached)
		fprintf(stderr, " (fetched from cache) isexec %u size %zu\n", file.isexec, file.file.len);
	else
		fprintf(stderr, " mode %.3o size %zu\n", file.mode, file.file.len);

	//helper_printf("M 100%.3o %s %s\n", hash, rev->path);
	helper_printf("M 100%.3o inline %s\n", file.isexec ? 0755 : 0644, rev->path);
	helper_printf("data %zu\n", file.file.len);
	helper_write(file.file.buf, file.file.len);
	helper_printf("\n");

	return 0;
}

static int markid = 0;
static int fast_export_cvs_commit(struct cvs_commit *ps, const char *branch_name, struct strbuf *parent_mark)
{
	/*
	 * TODO: clean extra lines in commit messages
	 */
	const char *author_ident;
	markid++;

	/*
	'commit' SP <ref> LF
	mark?
	('author' (SP <name>)? SP LT <email> GT SP <when> LF)?
	'committer' (SP <name>)? SP LT <email> GT SP <when> LF
	data
	('from' SP <committish> LF)?
	('merge' SP <committish> LF)?
	(filemodify | filedelete | filecopy | filerename | filedeleteall | notemodify)*
	LF?
	*/

	author_ident = author_convert(ps->author);
	if (!author_ident)
		die("failed to resolve cvs userid %s", ps->author);

	if (have_import_hook) {
		struct strbuf line = STRBUF_INIT;
		struct strbuf commit = STRBUF_INIT;
		FILE *fp;
		int rc;

		strbuf_addf(&commit, "author %s %ld +0000\n", author_ident, ps->timestamp);
		strbuf_addf(&commit, "committer %s %ld +0000\n", author_ident, ps->timestamp_last);
		strbuf_addf(&commit, "%s\n", ps->msg);

		fp = fopen(git_path(import_commit_edit), "w");
		if (fp == NULL)
			die_errno(_("could not open '%s'"), git_path(import_commit_edit));
		if (fwrite(commit.buf, 1, commit.len, fp) < commit.len)
			die("could not write %s", import_commit_edit);
		fclose(fp);

		rc = run_hook(NULL, "cvs-import-commit", git_path(import_commit_edit), NULL);
		if (rc)
			die("cvs-import-commit hook rc %d, abording import", rc);

		fp = fopen(git_path(import_commit_edit), "r");
		if (fp == NULL)
			die_errno(_("could not open '%s'"), git_path(import_commit_edit));

		helper_printf("commit %s%s\n", get_private_ref_prefix(), branch_name);
		helper_printf("mark :%d\n", markid);
		rc = strbuf_getline(&line, fp, '\n');
		if (rc)
			die("could not read %s", git_path(import_commit_edit));
		helper_printf("%s\n", line.buf); // author
		rc = strbuf_getline(&line, fp, '\n');
		if (rc)
			die("could not read %s", git_path(import_commit_edit));
		helper_printf("%s\n", line.buf); // committer

		strbuf_reset(&commit);
		while (!strbuf_getline(&line, fp, '\n')) {
			strbuf_addf(&commit, "%s\n", line.buf);
		}
		fclose(fp);

		helper_printf("data %zu\n", commit.len);
		helper_printf("%s\n", commit.buf);
		strbuf_release(&line);
		strbuf_release(&commit);
	}
	else {
		helper_printf("commit %s%s\n", get_private_ref_prefix(), branch_name);
		helper_printf("mark :%d\n", markid);
		//helper_printf("author %s <%s> %ld +0000\n", ps->author, "unknown", ps->timestamp);
		//helper_printf("committer %s <%s> %ld +0000\n", ps->author, "unknown", ps->timestamp_last);
		helper_printf("author %s %ld +0000\n", author_ident, ps->timestamp);
		helper_printf("committer %s %ld +0000\n", author_ident, ps->timestamp_last);
		helper_printf("data %zu\n", strlen(ps->msg));
		helper_printf("%s\n", ps->msg);

	}
	if (parent_mark->len)
		helper_printf("from %s\n", parent_mark->buf);
	for_each_hash(ps->revision_hash, fast_export_revision_cb, NULL);
	//helper_printf("\n");
	helper_flush();

	return markid;
}

static int fast_export_revision_meta_cb(void *ptr, void *data)
{
	struct cvs_revision *rev = ptr;
	struct strbuf *sb = data;

	format_add_meta_line(sb, rev);
	return 0;
}

static int print_revision_changes_cb(void *ptr, void *data)
{
	struct cvs_revision *rev = ptr;

	if (!rev->isdead)
		helper_printf("updated %s %s\n", rev->revision, rev->path);
	else
		helper_printf("deleted %s %s\n", rev->revision, rev->path);
	//helper_printf("%s:%c:%s\n", rev->revision, rev->isdead ? '-' : '+', rev->path);
	return 0;
}

static int fast_export_commit_meta(struct hash_table *meta, struct cvs_commit *ps, const char *branch_name, struct strbuf *commit_mark, struct strbuf *parent_mark)
{
	struct strbuf sb = STRBUF_INIT;

	markid++;
	helper_printf("commit %s%s\n", get_meta_ref_prefix(), branch_name);
	helper_printf("mark :%d\n", markid);
	//helper_printf("author %s <%s> %ld +0000\n", ps->author, "unknown", ps->timestamp);
	helper_printf("committer %s <%s> %ld +0000\n", ps->author, "unknown", ps->timestamp_last);
	helper_printf("data <<EOM\n");
	helper_printf("cvs meta update\n");
	for_each_hash(ps->revision_hash, print_revision_changes_cb, NULL);
	helper_printf("EOM\n");
	if (parent_mark->len)
		helper_printf("from %s\n", parent_mark->buf);
	helper_printf("N inline %s\n", commit_mark->buf);
	helper_printf("data <<EON\n");
	if (ps->cancellation_point)
		helper_printf("UPDATE:%ld\n", ps->cancellation_point);
	helper_printf("--\n");
	for_each_hash(meta, fast_export_revision_meta_cb, &sb);
	helper_printf("%s", sb.buf);
	helper_printf("EON\n");
	//helper_printf("\n");
	helper_flush();

	strbuf_release(&sb);
	return markid;
}

static int fast_export_blob(void *buf, size_t size)
{
	/*
	'blob' LF
	mark?
	'data' SP <count> LF
	<raw> LF?
	*/

	markid++;
	helper_printf("blob\n");
	helper_printf("mark :%d\n", markid);
	helper_printf("data %zu\n", size);
	helper_write(buf, size);
	helper_printf("\n");
	helper_flush();

	return markid;
}

static int update_revision_hash(void *ptr, void *data)
{
	struct cvs_revision *rev = ptr;
	struct hash_table *meta = data;

	unsigned int hash;
	void **pos;

	hash = hash_path(rev->path);
	pos = insert_hash(hash, rev, meta);
	if (pos) {
		struct cvs_revision *prev = *pos;

		if (strcmp(rev->path, prev->path))
			die("file path hash collision");

		*pos = rev;
	}
	return 0;
}

static void on_file_checkout_cb(struct cvsfile *file, void *data)
{
	struct hash_table *meta_revision_hash = data;
	int mark;

	/*
	 * FIXME: support files on disk
	 */
	if (!file->ismem)
		die("no support for files on disk yet");

	mark = fast_export_blob(file->file.buf, file->file.len);

	add_cvs_revision_hash(meta_revision_hash, file->path.buf, file->revision.buf, file->isdead, file->isexec, mark);
}

static int checkout_branch(const char *branch_name, time_t import_time, struct hash_table *meta_revision_hash)
{
	int rc;

	/*
	 * cvs checkout is broken, it uses command arguments from earilier
	 * commands. New session is needed.
	 */
	struct cvs_transport *cvs_co = cvs_connect(cvsroot, cvsmodule);
	if (!cvs_co)
		return -1;

	rc = cvs_checkout_branch(cvs_co, branch_name, import_time, on_file_checkout_cb, meta_revision_hash);
	if (rc)
		die("cvs checkout of %s date %ld failed", branch_name, import_time);

	return cvs_terminate(cvs_co);
}

static int count_dots(const char *rev)
{
	int dots = 0;

	while (1) {
		rev = strchr(rev, '.');
		if (!rev)
			break;

		dots++;
		rev++;
	}

	return dots;
}

static char *get_cvs_revision_branch(struct cvs_revision *file_meta)
{
	return cvs_get_rev_branch(cvs, file_meta->path, file_meta->revision);
}

struct find_rev_data {
	struct cvs_revision *file_meta;
	int dots;
};
static int find_longest_rev(void *ptr, void *data)
{
	struct cvs_revision *rev_meta = ptr;
	struct find_rev_data *find_rev_data = data;
	int dots;

	dots = count_dots(rev_meta->revision);
	if (dots > find_rev_data->dots) {
		find_rev_data->dots = dots;
		find_rev_data->file_meta = rev_meta;
	}

	return 0;
}

/*
 * FIXME: longest revision is not always parent
 */
static char *find_parent_branch(const char *branch_name, struct hash_table *meta_revision_hash)
{
	struct find_rev_data find_rev_data = { NULL, 0 };
	for_each_hash(meta_revision_hash, find_longest_rev, &find_rev_data);


	if (find_rev_data.dots == 0)
		die("longest revision is 0");

	if (find_rev_data.dots == 1)
		return xstrdup("HEAD");

	return get_cvs_revision_branch(find_rev_data.file_meta);
}

static int compare_commit_meta(unsigned char sha1[20], const char *meta_ref, struct hash_table *meta_revision_hash)
{
	struct cvs_revision *file_meta;
	char *buf;
	char *p;
	char *revision;
	char *path;
	char *attr;
	unsigned long size;
	int rev_mismatches = 0;
	int isdead;

	buf = read_note_of(sha1, meta_ref, &size);
	if (!buf)
		return -1;

	p = buf;
	while ((p = parse_meta_line(buf, size, &revision, &path, &attr, p))) {
		if (strcmp(revision, "--") == 0)
			break;
	}

	while ((p = parse_meta_line(buf, size, &revision, &path, &attr, p))) {
		if (!path || !attr)
			die("malformed metadata: %s:%s:%s", revision, attr, path);
		isdead = !!strstr(attr, "dead");

		file_meta = lookup_hash(hash_path(path), meta_revision_hash);
		if (!file_meta && isdead)
			continue;

		if (!file_meta ||
		    strcmp(file_meta->revision, revision))
			rev_mismatches++;
	}

	free(buf);
	return rev_mismatches;
}

static const char *find_branch_fork_point(const char *parent_branch_name, time_t time, struct hash_table *meta_revision_hash)
{
	unsigned char sha1[20];
	struct commit *commit;
	struct strbuf branch_ref = STRBUF_INIT;
	struct strbuf cvs_branch_ref = STRBUF_INIT;
	const char *commit_ref = NULL;
	int rev_mismatches_min = INT_MAX;
	int rev_mismatches;

	save_commit_buffer = 0;

	strbuf_addf(&branch_ref, "%s%s", get_private_ref_prefix(), parent_branch_name);
	strbuf_addf(&cvs_branch_ref, "%s%s", get_meta_ref_prefix(), parent_branch_name);

	if (get_sha1_commit(branch_ref.buf, sha1))
		die("cannot find last commit on branch ref %s", branch_ref.buf);

	commit = lookup_commit(sha1);

	for (;;) {
		if (parse_commit(commit))
			die("cannot parse commit %s", sha1_to_hex(commit->object.sha1));
		fprintf(stderr, "find_branch_fork_point: commit: %s date: %s commit: %p\n",
			sha1_to_hex(commit->object.sha1), show_date(commit->date, 0, DATE_NORMAL), commit);

		if (commit->date <= time) {
			rev_mismatches = compare_commit_meta(commit->object.sha1, cvs_branch_ref.buf, meta_revision_hash);
			fprintf(stderr, "rev_mismatches: %d\n", rev_mismatches);
			if (rev_mismatches == -1) {
				/*
				 * TODO: compare_commit_meta return -1 if no
				 * metadata note available for commit. Is this
				 * an error?
				 * commit_ref = NULL;
				 */
				break;
			}

			if (!rev_mismatches) {
				fprintf(stderr, "find_branch_fork_point - perfect match\n");
				commit_ref = sha1_to_hex(commit->object.sha1);
				break;
			}

			if (rev_mismatches_min < rev_mismatches)
				break;

			if (rev_mismatches_min > rev_mismatches) {
				rev_mismatches_min = rev_mismatches;
				commit_ref = sha1_to_hex(commit->object.sha1);
			}
		}

		if (!commit->parents)
			break;
		commit = commit->parents->item;
	}

	strbuf_release(&branch_ref);
	strbuf_release(&cvs_branch_ref);
	return commit_ref;
}

static int fast_export_revision_by_mark(void *ptr, void *data)
{
	struct cvs_revision *rev = ptr;

	helper_printf("M 100%.3o :%d %s\n", rev->isexec ? 0755 : 0644, rev->mark, rev->path);
	//helper_printf("\n");

	return 0;
}

static int fast_export_branch_initial(struct hash_table *meta_revision_hash,
		const char *branch_name, time_t date, const char *parent_commit_ref,
		const char *parent_branch_name)
{
	markid++;

	helper_printf("commit %s%s\n", get_private_ref_prefix(), branch_name);
	helper_printf("mark :%d\n", markid);
	helper_printf("author git-remote-cvs <none> %ld +0000\n", date);
	helper_printf("committer git-remote-cvs <none> %ld +0000\n", date);
	helper_printf("data <<EON\n");
	helper_printf("initial import of branch: %s\nparent branch: %s\n", branch_name, parent_branch_name);
	helper_printf("EON\n");
	//helper_printf("merge %s\n", parent_commit_ref);
	helper_printf("from %s\n", parent_commit_ref);
	helper_printf("deleteall\n");
	for_each_hash(meta_revision_hash, fast_export_revision_by_mark, NULL);
	helper_flush();

	return markid;
}

static int make_initial_branch_import(const char *branch_name, struct cvs_branch *cvs_branch)
{
	int rc;
	int mark;
	char *parent_branch_name;
	struct string_list_item *item;
	const char *parent_commit;
	time_t import_time = find_first_commit_time(cvs_branch);
	if (!import_time)
		die("import time is 0");
	import_time--;
	fprintf(stderr, "import time is %s\n", show_date(import_time, 0, DATE_RFC2822));

	rc = checkout_branch(branch_name, import_time, cvs_branch->last_commit_revision_hash);
	if (rc == -1)
		die("initial branch checkout failed %s", branch_name);

	if (is_empty_hash(cvs_branch->last_commit_revision_hash))
		return 0;

	parent_branch_name = find_parent_branch(branch_name, cvs_branch->last_commit_revision_hash);
	if (!parent_branch_name)
		die("Cannot find parent branch for: %s", branch_name);
	fprintf(stderr, "PARENT BRANCH FOR: %s is %s\n", branch_name, parent_branch_name);
	/*
	 * if parent is not updated yet, import parent first
	 */
	item = unsorted_string_list_lookup(import_branch_list, parent_branch_name);
	if (item && !item->util) {
		fprintf(stderr, "fetching parent first\n");
		import_branch_by_name(item->string);
		item->util = (void*)1;
	}

	parent_commit = find_branch_fork_point(parent_branch_name,
						import_time,
						cvs_branch->last_commit_revision_hash);
	fprintf(stderr, "PARENT COMMIT: %s\n", parent_commit);

	mark = fast_export_branch_initial(cvs_branch->last_commit_revision_hash,
					branch_name,
					import_time,
					parent_commit,
					parent_branch_name);
	// commit_meta
	free(parent_branch_name);
	return mark;
}

static void merge_revision_hash(struct hash_table *meta, struct hash_table *update)
{
	for_each_hash(update, update_revision_hash, meta);
}

static int import_branch_by_name(const char *branch_name)
{
	static int mark;
	unsigned char sha1[20];
	struct strbuf commit_mark_sb = STRBUF_INIT;
	struct strbuf meta_mark_sb = STRBUF_INIT;
	struct strbuf branch_ref = STRBUF_INIT;
	struct strbuf branch_private_ref = STRBUF_INIT;
	struct strbuf meta_branch_ref = STRBUF_INIT;
	struct hash_table meta_revision_hash;
	struct string_list_item *li;

	fprintf(stderr, "importing CVS branch %s\n", branch_name);

	struct cvs_branch *cvs_branch;
	int psnum = 0;
	int pstotal = 0;

	strbuf_addf(&branch_ref, "%s%s", get_ref_prefix(), branch_name);
	strbuf_addf(&meta_branch_ref, "%s%s", get_meta_ref_prefix(), branch_name);

	li = unsorted_string_list_lookup(&cvs_branch_list, branch_name);
	if (!li && !ref_exists(meta_branch_ref.buf))
		die("Cannot find meta for branch %s\n", branch_name);
	cvs_branch = li->util;

	/*
	 * FIXME: support of repositories with no files
	 */
	if (is_empty_hash(cvs_branch->last_commit_revision_hash) &&
	    strcmp(branch_name, "HEAD")) {
		/*
		 * no meta, do cvs checkout
		 */
		mark = make_initial_branch_import(branch_name, cvs_branch);
		if (mark == -1)
			die("make_initial_branch_import failed %s", branch_name);
		if (mark > 0)
			strbuf_addf(&commit_mark_sb, ":%d", mark);
	}
	else {
		if (!get_sha1(branch_ref.buf, sha1))
			strbuf_addstr(&commit_mark_sb, sha1_to_hex(sha1));
		if (!get_sha1(meta_branch_ref.buf, sha1))
			strbuf_addstr(&meta_mark_sb, sha1_to_hex(sha1));
	}
	aggregate_cvs_commits(cvs_branch);

	init_hash(&meta_revision_hash);
	merge_revision_hash(&meta_revision_hash, cvs_branch->last_commit_revision_hash);

	cvs_authors_load();

	pstotal = get_cvs_commit_count(cvs_branch);
	struct cvs_commit *ps = cvs_branch->cvs_commit_list->head;
	while (ps) {
		psnum++;
		fprintf(stderr, "-->>------------------\n");
		fprintf(stderr, "Branch: %s Commit: %d/%d\n", branch_name, psnum, pstotal);
		print_cvs_commit(ps);
		fprintf(stderr, "--<<------------------\n\n");
		mark = fast_export_cvs_commit(ps, branch_name, &commit_mark_sb);
		strbuf_reset(&commit_mark_sb);
		strbuf_addf(&commit_mark_sb, ":%d", mark);

		merge_revision_hash(&meta_revision_hash, ps->revision_hash);
		mark = fast_export_commit_meta(&meta_revision_hash, ps, branch_name, &commit_mark_sb, &meta_mark_sb);
		strbuf_reset(&meta_mark_sb);
		strbuf_addf(&meta_mark_sb, ":%d", mark);

		ps = ps->next;
	}
	if (psnum) {
		fprintf(stderr, "Branch: %s Commits number: %d\n", branch_name, psnum);

		if (initial_import && !strcmp(branch_name, "HEAD")) {
			helper_printf("reset HEAD\n");
			helper_printf("from %s\n", commit_mark_sb.buf);
		}

		helper_printf("checkpoint\n");
		helper_flush();
		/*
		 * FIXME: sync with fast-export
		 */
		invalidate_ref_cache(NULL);
	}
	else {
		fprintf(stderr, "Branch: %s is up to date\n", branch_name);
	}

	strbuf_release(&commit_mark_sb);
	strbuf_release(&meta_mark_sb);
	strbuf_release(&branch_ref);
	strbuf_release(&branch_private_ref);
	strbuf_release(&meta_branch_ref);
	free_hash(&meta_revision_hash);
	return 0;
}

static int cmd_batch_import(struct string_list *list)
{
	struct string_list_item *item;
	const char *branch_name;

	for_each_string_list_item(item, list) {
		if (!(branch_name = gettext_after(item->string, "import refs/heads/")))
			die("Malformed import command (wrong ref prefix) %s", item->string);

		memmove(item->string, branch_name, strlen(branch_name) + 1); // move including \0
	}

	import_branch_list = list;
	import_start_time = time(NULL);

	//progress_state = start_progress("Receiving revisions", revisions_all_branches_total);

	item = unsorted_string_list_lookup(list, "HEAD");
	if (item) {
		import_branch_by_name(item->string);
		item->util = (void*)1;
	}

	for_each_string_list_item(item, list) {
		if (!item->util) {
			import_branch_by_name(item->string);
			item->util = (void*)1;
		}
	}

	helper_printf("done\n");
	helper_flush();
	//stop_progress(&progress_state);
	return 0;
}

/*
 * iterates over commits until it finds the one with cvs metadata (point where
 * git local topic branch was started)
 */
static int prepare_push_commit_list(unsigned char *sha1, const char *meta_ref, struct commit_list **push_list)
{
	struct hash_table *revision_meta_hash;
	struct commit *commit;

	*push_list = NULL;

	commit = lookup_commit(sha1);
	for (;;) {
		if (parse_commit(commit))
			die("cannot parse commit %s", sha1_to_hex(commit->object.sha1));

		fprintf(stderr, "adding push list commit: %s date: %s commit: %p\n",
			sha1_to_hex(commit->object.sha1), show_date(commit->date, 0, DATE_NORMAL), commit);
		commit_list_insert(commit, push_list);
		load_revision_meta(commit->object.sha1, meta_ref, NULL, &revision_meta_hash);
		if (revision_meta_hash) {
			commit->util = revision_meta_hash;
			fprintf(stderr, "adding push list commit meta: %p\n", commit);
			break;
		}

		if (!commit->parents)
			break;
		if (commit_list_count(commit->parents) > 1)
			die("pushing of merge commits is not supported");

		commit = commit->parents->item;
	}

	if (!revision_meta_hash) {
		free_commit_list(*push_list);
		*push_list = NULL;
		return -1;
	}

	return 0;
}

/*
 * TODO: move this stuff somewhere. Pack in structure and add util to diff_options?
 */
static struct string_list new_directory_list = STRING_LIST_INIT_DUP; // used to create non-existing directories
static struct string_list touched_file_list = STRING_LIST_INIT_DUP; // used for status before pushing changes
static struct hash_table *base_revision_meta_hash = NULL;
static struct commit *current_commit = NULL; // used to save string_list of file path / cvsfile per commit
struct sha1_mod {
	unsigned char sha1[20];
	unsigned mode;
	int addremove;
};

static struct sha1_mod *make_sha1_mod(const unsigned char *sha1, unsigned mode, int addremove)
{
	struct sha1_mod *sm = xmalloc(sizeof(*sm));
	hashcpy(sm->sha1, sha1);
	sm->mode = mode;
	sm->addremove = addremove;

	return sm;
}

static void add_commit_file(struct commit *commit, const char *path, const unsigned char *sha1, unsigned mode, int addremove)
{
	struct string_list *file_list = current_commit->util;
	if (!file_list) {
		file_list = xcalloc(1, sizeof(*file_list));
		file_list->strdup_strings = 1;
		current_commit->util = file_list;
	}

	string_list_append(file_list, path)->util = make_sha1_mod(sha1, mode, addremove);;
}

static void on_file_change(struct diff_options *options,
			   unsigned old_mode, unsigned new_mode,
			   const unsigned char *old_sha1,
			   const unsigned char *new_sha1,
			   int old_sha1_valid, int new_sha1_valid,
			   const char *concatpath,
			   unsigned old_dirty_submodule, unsigned new_dirty_submodule)
{
	struct cvs_revision *rev;

	if (S_ISLNK(new_mode))
		die("CVS does not support symlinks: %s", concatpath);

	if ((S_ISDIR(old_mode) && !S_ISDIR(new_mode)) ||
	    (!S_ISDIR(old_mode) && S_ISDIR(new_mode)))
		die("CVS cannot handle file paths which used to de directories and vice versa: %s", concatpath);

	if (S_ISDIR(new_mode))
		return;

	if (!S_ISREG(new_mode))
		die("CVS cannot handle non-regular files: %s", concatpath);

	if (old_mode != new_mode && !ignore_mode_change)
		die("CVS does not support file permission changes. "
		    "Set cvshelper.ignoreModeChange to true if you want to ignore and push anyway.");

	fprintf(stderr, "------\nfile changed: %s "
			"mode: %o -> %o "
			"sha: %d %s -> %d %s\n",
			concatpath,
			old_mode, new_mode,
			old_sha1_valid,
			sha1_to_hex(old_sha1),
			new_sha1_valid,
			sha1_to_hex(new_sha1));

	/*
	 * TODO: verify that old meta exists
	 * - note case when file added in previous commit and no metadata exist
	 */
	rev = lookup_hash(hash_path(concatpath), base_revision_meta_hash);
	if (!rev) {
		/*
		 * FIXME:
		 */
	}
	string_list_append(&touched_file_list, concatpath);
	add_commit_file(current_commit, concatpath, new_sha1, new_mode, 0);
}

static void on_file_addremove(struct diff_options *options,
			      int addremove, unsigned mode,
			      const unsigned char *sha1,
			      int sha1_valid,
			      const char *concatpath, unsigned dirty_submodule)
{
	if (S_ISLNK(mode))
		die("CVS does not support symlinks: %s", concatpath);

	fprintf(stderr, "------\n%s %s: %s "
			"mode: %o "
			"sha: %d %s\n",
			S_ISDIR(mode) ? "dir" : "file",
			addremove == '+' ? "add" : "remove",
			concatpath,
			mode,
			sha1_valid,
			sha1_to_hex(sha1));

	if (S_ISDIR(mode)) {
		if (addremove == '+')
			string_list_append(&new_directory_list, concatpath);
		return;
	}

	if (!S_ISREG(mode))
		die("CVS cannot handle non-regular files: %s", concatpath);

	/*
	 * FIXME: meta needed?
	 */
	string_list_append(&touched_file_list, concatpath);
	/*
	 * FIXME: add/remove flag
	 */
	add_commit_file(current_commit, concatpath, sha1, mode, addremove);
}

static int check_file_list_remote_status(const char *cvs_branch, struct string_list *file_list, struct hash_table *revision_meta_hash)
{
	struct cvsfile *files;
	struct cvs_revision *rev;
	int count = file_list->nr;
	int rc;
	int i;

	files = xcalloc(count, sizeof(*files));
	for (i = 0; i < count; i++) {
		cvsfile_init(&files[i]);
		strbuf_addstr(&files[i].path, file_list->items[i].string);

		rev = lookup_hash(hash_path(files[i].path.buf), base_revision_meta_hash);
		if (rev)
			strbuf_addstr(&files[i].revision, rev->revision);
		else
			files[i].isnew = 1;
		/*
		 * FIXME:
		 * What to do with new/remove files?
		 */

		fprintf(stderr, "status: %s rev: %s\n", files[i].path.buf, files[i].revision.buf);
	}

	rc = cvs_status(cvs, cvs_branch, files, count);
	for (i = 0; i < count; i++)
		cvsfile_release(&files[i]);
	free(files);
	return rc;
}

static int create_remote_directories(const char *cvs_branch, struct string_list *new_directory_list)
{
	struct string_list_item *item;
	for_each_string_list_item(item, new_directory_list) {
		fprintf(stderr, "directory add %s\n", item->string);
	}
	return cvs_create_directories(cvs, cvs_branch, new_directory_list);
}

static int prepare_file_content(struct cvsfile *file, void *data)
{
	enum object_type type;
	void *buf;
	unsigned long size;
	struct sha1_mod *sm;

	sm = file->util;
	if (!sm)
		die("Cannot prepare file content for commit, no sha1 known");

	if (sm->addremove == '-')
		return 0;

	buf = read_sha1_file(sm->sha1, &type, &size);
	if (!buf)
		return -1;

	/*
	 * FIXME: no reallocs
	 */
	strbuf_attach(&file->file, buf, size, size);
	return 0;
}

static void release_file_content(struct cvsfile *file, void *data)
{
	strbuf_release(&file->file);
}

static char *run_export_hook(const char *hook_message)
{
	struct strbuf sb = STRBUF_INIT;
	FILE *fp;
	int rc;

	fp = fopen(git_path(export_commit_edit), "w");
	if (fp == NULL)
		die_errno(_("could not open '%s'"), git_path(export_commit_edit));
	if (fwrite(hook_message, 1, strlen(hook_message), fp) < strlen(hook_message))
		die("could not write %s", export_commit_edit);
	fclose(fp);

	rc = run_hook(NULL, "cvs-export-commit", git_path(export_commit_edit), NULL);
	if (rc)
		die("cvs-export-commit hook rc %d, abording export", rc);

	fp = fopen(git_path(export_commit_edit), "r");
	if (fp == NULL)
		die_errno(_("could not open '%s'"), git_path(export_commit_edit));

	if (strbuf_read_file(&sb, git_path(export_commit_edit), 0) < 0)
		die_errno(_("could not read '%s'"), git_path(export_commit_edit));
	fclose(fp);
	return strbuf_detach(&sb, NULL);
}

static int push_commit_to_cvs(struct commit *commit, const char *cvs_branch, struct hash_table *revision_meta_hash)
{
	struct cvs_revision *rev;
	struct string_list *file_list;
	struct string_list_item *item;
	struct cvsfile *files;
	struct sha1_mod *sm;
	const char *commit_message;
	struct strbuf meta_ref_sb = STRBUF_INIT;
	struct strbuf meta_commit_msg_sb = STRBUF_INIT;
	char *hook_message = NULL;
	int count;
	int rc;
	int i;
	int need_new_cvs_session = 0;
	if (!commit->util)
		return -1;

	fprintf(stderr, "pushing commit %s to CVS branch %s\n", sha1_to_hex(commit->object.sha1), cvs_branch);
	strbuf_addstr(&meta_commit_msg_sb, "cvs meta update\n");

	file_list = commit->util;
	sort_string_list(file_list);
	count = file_list->nr;

	files = xcalloc(count, sizeof(*files));
	for (i = 0; i < count; i++) {
		cvsfile_init(&files[i]);
		item = &file_list->items[i];
		sm = item->util;
		if (!sm)
			die("No sha1 mod accociated with file being checked in");

		files[i].util = sm;
		files[i].isexec = !!(sm->mode & 0111);
		files[i].mode = sm->mode;
		fprintf(stderr, "check in %c file: %s sha1: %s mod: %.4o\n",
				sm->addremove ? sm->addremove : '*', item->string, sha1_to_hex(sm->sha1), sm->mode);
		strbuf_addstr(&files[i].path, item->string);
		if (sm->addremove == '+') {
			files[i].isnew = 1;
		}
		else if (sm->addremove == '-') {
			files[i].isdead = 1;
			need_new_cvs_session = 1;
		}

		rev = lookup_hash(hash_path(files[i].path.buf), revision_meta_hash);
		if (rev) {
			strbuf_addstr(&files[i].revision, rev->revision);
			if (files[i].isnew)
				warning("file: %s meta rev: %s is supposed to be new, but revision metadata was found",
					files[i].path.buf, files[i].revision.buf);
		}
		else {
			if (files[i].isnew)
				add_cvs_revision_hash(revision_meta_hash, files[i].path.buf, "0", 0, 1, 0);
			else
				die("file: %s has not revision metadata, and not new", files[i].path.buf);
		}
	}

	find_commit_subject(commit->buffer, &commit_message);
	fprintf(stderr, "export hook '%s'\n", commit->buffer);
	if (have_export_hook) {
		hook_message = strstr(commit->buffer, "author ");
		if (!hook_message)
			die("unexpected commit format '%s'", commit->buffer);
		commit_message = hook_message = run_export_hook(hook_message);
	}

	/*
	 * FIXME: CVS cannot handle few commits during single session sometimes.
	 * Something has to do with same files added/modified/deleted. Try to
	 * minimize new cvs sessions.
	 */
	if (need_new_cvs_session) {
		fprintf(stderr, "extra cvs session\n");
		rc = cvs_terminate(cvs);
		if (rc)
			die("ungraceful cvs session termination");
		cvs = cvs_connect(cvsroot, cvsmodule);
		if (!cvs)
			die("failed to establish new cvs session");
	}
	rc = cvs_checkin(cvs, cvs_branch, commit_message, files, count, prepare_file_content, release_file_content, NULL);
	if (!rc) {
		for (i = 0; i < count; i++) {
			rev = lookup_hash(hash_path(files[i].path.buf), revision_meta_hash);
			if (!rev)
				die("commit succeeded, but cannot find meta file to update: %s", files[i].path.buf);
			if (!rev->revision)
				die("commit succeeded, but cannot find meta file revision to update: %s", files[i].path.buf);
			free(rev->revision);
			rev->revision = strbuf_detach(&files[i].revision, NULL);
			rev->isdead = files[i].isdead;
			//fprintf(stderr, "new rev: %s %s\n", rev->revision, rev->path);
			/*if (!files[i].isdead)
				rev->revision = strbuf_detach(&files[i].revision, NULL);
			else
				rev->revision = NULL;*/
			strbuf_addf(&meta_commit_msg_sb, "%s %s %s\n",
							 rev->isdead ? "deleted" : "updated",
							 rev->revision,
							 rev->path);
		}
		/*
		 * save metadata
		 */
		strbuf_addf(&meta_ref_sb, "%s%s", get_meta_ref_prefix(), cvs_branch);
		save_revision_meta(commit->object.sha1, meta_ref_sb.buf, meta_commit_msg_sb.buf, revision_meta_hash);
	}

	for (i = 0; i < count; i++)
		cvsfile_release(&files[i]);
	free(files);
	strbuf_release(&meta_ref_sb);
	strbuf_release(&meta_commit_msg_sb);
	if (hook_message)
		free(hook_message);
	return rc;
}

static int push_commit_list_to_cvs(struct commit_list *push_list, const char *cvs_branch)
{
	struct commit *commit;
	struct commit *parent;
	struct commit *base;
	struct commit_list *push_list_it;
	struct hash_table *revision_meta_hash;

	struct diff_options diffopt;
	diff_setup(&diffopt);
	diffopt.change = on_file_change;
	diffopt.add_remove = on_file_addremove;
	DIFF_OPT_SET(&diffopt, RECURSIVE);
	DIFF_OPT_SET(&diffopt, TREE_IN_RECURSIVE);
	DIFF_OPT_SET(&diffopt, IGNORE_SUBMODULES);

	base = push_list->item;
	revision_meta_hash = base->util;
	if (!revision_meta_hash)
		die("push failed: base commit does not have CVS metadata");

	fprintf(stderr, "base commit: %s date: %s on CVS branch: %s\n",
			sha1_to_hex(base->object.sha1), show_date(base->date, 0, DATE_NORMAL), cvs_branch);

	base_revision_meta_hash = revision_meta_hash;
	string_list_clear(&new_directory_list, 0);
	string_list_clear(&touched_file_list, 0);

	push_list_it = push_list;
	while ((push_list_it = push_list_it->next)) {
		commit = push_list_it->item;
		parent = commit->parents->item;

		fprintf(stderr, "\n-----------------------------\npushing: %s date: %s to CVS branch: %s\n",
			sha1_to_hex(commit->object.sha1), show_date(commit->date, 0, DATE_NORMAL), cvs_branch);

		current_commit = commit;
		diff_tree_sha1(parent->object.sha1, commit->object.sha1, "", &diffopt);
	}

	sort_string_list(&new_directory_list);
	string_list_remove_duplicates(&new_directory_list, 0);
	sort_string_list(&touched_file_list);
	string_list_remove_duplicates(&touched_file_list, 0);

	/*
	 * filtered later
	 *
	push_list_it = push_list;
	while ((push_list_it = push_list_it->next)) {
		commit = push_list_it->item;
		if (!commit->util)
			die("No files changed in pending commit: %s", sha1_to_hex(commit->object.sha1));
	}
	*/
	if (new_directory_list.nr &&
	    create_remote_directories(cvs_branch, &new_directory_list))
		return 1;

	if (check_file_list_remote_status(cvs_branch, &touched_file_list, revision_meta_hash)) {
		error("You are not up-to date. Update your local repository copy first.");
		return 1;
	}

	push_list_it = push_list;
	while ((push_list_it = push_list_it->next)) {
		commit = push_list_it->item;
		if (commit->util) {
			if (push_commit_to_cvs(commit, cvs_branch, revision_meta_hash))
				return 1;
		}
		else {
			warning("Skipping empty commit: %s", sha1_to_hex(commit->object.sha1));
		}
	}
	/*
	 * TODO:
	 * - update metadata???
	 * - commit list cleanup
	 * - util in commit list cleanup
	 */
	push_list_it = push_list;
	while ((push_list_it = push_list_it->next)) {
		commit = push_list_it->item;
		if (commit->util) {
			string_list_clear(commit->util, 1);
			free(commit->util);
			commit->util = 0;
		}
	}

	return 0;
}

/*
 * TODO:
 *  - do new branch creation on force push
 *  - tag support
 */
static int push_branch(const char *src, const char *dst, int force)
{
	int rc = -1;
	const char *cvs_branch;
	struct strbuf meta_ref_sb = STRBUF_INIT;

	cvs_branch = strrchr(dst, '/');
	if (!cvs_branch)
		die("Malformed destination branch name");
	cvs_branch++;

	fprintf(stderr, "pushing %s to %s (CVS branch: %s) force: %d\n", src, dst, cvs_branch, force);

	unsigned char sha1[20];
	if (get_sha1(src, sha1))
		die(_("Failed to resolve '%s' as a valid ref."), src);

	strbuf_addf(&meta_ref_sb, "%s%s", get_meta_ref_prefix(), cvs_branch);
	if (!ref_exists(meta_ref_sb.buf))
		die("No metadata for CVS branch %s. No support for new CVS branch creation yet", cvs_branch);

	struct commit_list *push_list = NULL;

	if (prepare_push_commit_list(sha1, meta_ref_sb.buf, &push_list))
		die("prepare_push_commit_list failed");

	/*
	 * prepare_push_commit_list always put commit with cvs metadata first,
	 * that should not be pushed
	 */
	if (commit_list_count(push_list) > 1) {
		if (!push_commit_list_to_cvs(push_list, cvs_branch)) {
			if (no_refs_update_on_push)
				strbuf_addstr(&push_error_sb, "NO_REFS_UPDATE_ON_PUSH "
					      "was set. Perform fetch to get pushed changes from CVS.");
			else
				rc = 0;
		}
	}
	else {
		fprintf(stderr, "Nothing to push");
		rc = 0;
	}

	free_commit_list(push_list);
	strbuf_release(&meta_ref_sb);
	return rc;
}

static int cmd_batch_push(struct string_list *list)
{
	struct string_list_item *item;
	const char *srcdst;
	const char *src;
	const char *dst;
	char *p;
	int force;

	for_each_string_list_item(item, list) {
		if (!(srcdst = gettext_after(item->string, "push ")) ||
		    !strchr(srcdst, ':'))
			die("Malformed push command: %s", item->string);

		memmove(item->string, srcdst, strlen(srcdst) + 1); // move including \0
	}

	//import_branch_list = list;
	//import_start_time = time(NULL);

	//progress_state = start_progress("Receiving revisions", revisions_all_branches_total);

	for_each_string_list_item(item, list) {
		srcdst = item->string;
		force = 0;
		if (srcdst[0] == '+') {
			force = 1;
			srcdst++;
		}
		p = strchr(srcdst, ':');
		if (!p)
			die("Malformed push source-destination specification: %s", srcdst);
		*p++ = '\0';

		src = srcdst;
		dst = p;

		strbuf_reset(&push_error_sb);
		if (!push_branch(src, dst, force)) {
			helper_printf("ok %s\n", dst);
		}
		else {
			if (!push_error_sb.len)
				strbuf_addstr(&push_error_sb, "pushing to CVS failed");
			helper_printf("error %s %s\n", dst, push_error_sb.buf);
		}
	}

	strbuf_release(&push_error_sb);
	helper_printf("\n");
	helper_flush();
	//stop_progress(&progress_state);
	return 0;
}

static void add_cvs_revision_cb(const char *branch_name,
			  const char *path,
			  const char *revision,
			  const char *author,
			  const char *msg,
			  time_t timestamp,
			  int isdead,
			  void *data) {
	struct string_list *cvs_branch_list = data;
	struct string_list_item *li;
	struct cvs_branch *cvs_branch;

	li = unsorted_string_list_lookup(cvs_branch_list, branch_name);
	if (!li) {
		cvs_branch = new_cvs_branch(branch_name);
		string_list_append(cvs_branch_list, branch_name)->util = cvs_branch;
	}
	else {
		cvs_branch = li->util;
	}

	skipped += add_cvs_revision(cvs_branch, path, revision, author, msg, timestamp, isdead);
	revisions_all_branches_total++;
	//display_progress(progress_rlog, revisions_all_branches_total);
}

static time_t update_since = 0;
static int on_each_ref(const char *branch_name, const unsigned char *sha1, int flags, void *data)
{
	struct string_list *cvs_branch_list = data;
	struct string_list_item *li;
	struct cvs_branch *cvs_branch;

	li = unsorted_string_list_lookup(cvs_branch_list, branch_name);
	if (!li) {
		cvs_branch = new_cvs_branch(branch_name);
		string_list_append(cvs_branch_list, branch_name)->util = cvs_branch;
	}
	else {
		cvs_branch = li->util;
	}

	if (!update_since ||
	    //FIXME: remember last update date somewhere or take last from
	    //branch: update_since < cvs_branch->last_revision_timestamp)
	    update_since < cvs_branch->last_revision_timestamp)
	    //update_since > cvs_branch->last_revision_timestamp)
		update_since = cvs_branch->last_revision_timestamp;
	return 0;
}

static int cmd_list(const char *line)
{
	int rc;
	cvs = cvs_connect(cvsroot, cvsmodule);
	if (!cvs)
		return -1;

	fprintf(stderr, "connected to cvs server\n");

	struct string_list_item *li;
	struct cvs_branch *cvs_branch;

	//progress_rlog = start_progress("revisions info", 0);
	if (initial_import) {
		rc = cvs_rlog(cvs, 0, 0, add_cvs_revision_cb, &cvs_branch_list);
		if (rc == -1)
			die("rlog failed");
		fprintf(stderr, "Total revisions: %d\n", revisions_all_branches_total);
		fprintf(stderr, "Skipped revisions: %d\n", skipped);

		for_each_string_list_item(li, &cvs_branch_list) {
			cvs_branch = li->util;
			finalize_revision_list(cvs_branch);
			if (cvs_branch->rev_list->nr)
				helper_printf("? refs/heads/%s\n", li->string);
		}
		helper_printf("\n");
	}
	else {
		for_each_ref_in(get_meta_ref_prefix(), on_each_ref, &cvs_branch_list);
		/*
		 * handle case when last rlog didn't pick all files committed
		 * last second (hit during autotests)
		 */
		if (update_since > 0)
			update_since--;
		fprintf(stderr, "update since: %ld\n", update_since);

		/*
		 * FIXME: we'll skip branches which were not imported during
		 * initial import. If some changes for these branches arrives
		 * and import will pick it up this time, then history will be
		 * truncated. Have to detect this case and do full rlog for
		 * importing such branches.
		 */
		rc = cvs_rlog(cvs, update_since, 0, add_cvs_revision_cb, &cvs_branch_list);
		if (rc == -1)
			die("rlog failed");
		fprintf(stderr, "Total revisions: %d\n", revisions_all_branches_total);
		fprintf(stderr, "Skipped revisions: %d\n", skipped);

		for_each_string_list_item(li, &cvs_branch_list) {
			cvs_branch = li->util;
			finalize_revision_list(cvs_branch);
			if (cvs_branch->rev_list->nr)
				helper_printf("? refs/heads/%s\n", li->string);
			else
				fprintf(stderr, "Branch: %s is up to date\n", li->string);
		}

		helper_printf("\n");
	}
	//stop_progress(&progress_rlog);
	helper_flush();
	revisions_all_branches_total -= skipped;
	return 0;
}

static int print_meta_branch_name(const char *branch_name, const unsigned char *sha1, int flags, void *data)
{
	helper_printf("? refs/heads/%s\n", branch_name);
	return 0;
}

static int cmd_list_for_push(const char *line)
{
	cvs = cvs_connect(cvsroot, cvsmodule);
	if (!cvs)
		return -1;

	fprintf(stderr, "connected to cvs server\n");

	//progress_rlog = start_progress("revisions info", 0);
	for_each_ref_in(get_meta_ref_prefix(), print_meta_branch_name, NULL);
	helper_printf("\n");

	//stop_progress(&progress_rlog);
	helper_flush();
	return 0;
}

static int validate_tree_entry(const unsigned char *sha1, const char *base,
		int baselen, const char *filename, unsigned mode, int stage,
		void *meta)
{
	struct cvs_revision *file_meta;
	struct hash_table *revision_meta_hash = meta;
	char path[PATH_MAX];

	if (S_ISDIR(mode))
		return READ_TREE_RECURSIVE;

	snprintf(path, sizeof(path), "%s%s", base, filename);
	//fprintf(stderr, "validating: %s\n", path);
	file_meta = lookup_hash(hash_path(path), revision_meta_hash);
	if (!file_meta)
		die("no meta for file %s\n", path);
	if (file_meta->isdead)
		die("file %s is dead in meta\n", path);

	file_meta->util = 1; // mark revision is visited
	return 0;
}

static int zero_cvs_revision_util(void *rev, void *data)
{
	struct cvs_revision *file_meta = rev;
	file_meta->util = 0;

	return 0;
}

static int validate_cvs_revision_util(void *rev, void *data)
{
	struct cvs_revision *file_meta = rev;

	if (!file_meta->isdead &&
	    !file_meta->util)
		die("file %s exists in meta, but not in tree", file_meta->path);

	return 0;
}

static int validate_commit_meta_by_tree(const char *ref, struct hash_table *revision_meta_hash)
{
	struct pathspec pathspec;
	struct commit *commit;
	unsigned char sha1[20];
	int err;

	fprintf(stderr, "validating commit meta by tree\n");

	if (!ref_exists(ref))
		die("ref does not exist %s", ref);

	//if (get_sha1_commit(ref, sha1))
	if (get_sha1(ref, sha1))
		die("cannot find last commit on branch ref %s", ref);

	commit = lookup_commit(sha1);
	if (parse_commit(commit))
		die("cannot parse commit %s", sha1_to_hex(sha1));

	for_each_hash(revision_meta_hash, zero_cvs_revision_util, NULL);

	init_pathspec(&pathspec, NULL);
	err = read_tree_recursive(commit->tree, "", 0, 0, &pathspec,
				  validate_tree_entry, revision_meta_hash);
	free_pathspec(&pathspec);
	if (err == READ_TREE_RECURSIVE)
		err = 0;

	for_each_hash(revision_meta_hash, validate_cvs_revision_util, NULL);
	return err;
}

/*static int validate_cvs_branch_meta_by_tree(const char *branch_name)
{
	struct strbuf ref_sb = STRBUF_INIT;
	struct strbuf meta_ref_sb = STRBUF_INIT;
	struct hash_table *revision_meta_hash = NULL;
	unsigned char commit_sha1[20];
	int rc;

	strbuf_addf(&ref_sb, "%s%s", get_ref_prefix(), branch_name);
	strbuf_addf(&meta_ref_sb, "%s%s", get_meta_ref_prefix(), branch_name);

	if (get_sha1(ref_sb.buf, commit_sha1))
		die(_("Failed to resolve '%s' as a valid ref."), ref_sb.buf);

	if (!ref_exists(meta_ref_sb.buf))
		die("No metadata for branch %s", branch_name);

	load_revision_meta(commit_sha1, meta_ref_sb.buf, NULL, &revision_meta_hash);
	if (!revision_meta_hash)
		die("Cannot load metadata for branch %s", branch_name);

	rc = validate_commit_meta_by_tree(ref_sb.buf, revision_meta_hash);
	strbuf_release(&ref_sb);
	strbuf_release(&meta_ref_sb);
	return rc;
}*/

static void on_every_file_revision(const char *path, const char *revision, void *meta)
{
	struct cvs_revision *file_meta;
	struct hash_table *revision_meta_hash = meta;

	fprintf(stderr, "rls: validating: %s\n", path);
	file_meta = lookup_hash(hash_path(path), revision_meta_hash);
	if (!file_meta)
		die("no meta for file %s\n", path);
	if (file_meta->isdead)
		die("file %s is dead in meta\n", path);
	if (strcmp(revision, file_meta->revision))
		die("file %s revision is wrong: meta %s cvs %s\n",
		    path, file_meta->revision, revision);

	file_meta->util = 1; // mark revision is visited
}

/*static int validate_cvs_branch_meta_by_cvs_rls(const char *branch_name)
{
	struct cvs_transport *cvs = cvs_connect(cvsroot, cvsmodule);
	if (!cvs)
		return -1;

	return cvs_rls(cvs, branch_name, 0, 0, on_every_file_revision, NULL);
}*/

static int validate_commit_meta_by_rls(const char *branch_name, time_t timestamp, struct hash_table *revision_meta_hash)
{
	int rc;
	struct cvs_transport *cvs = cvs_connect(cvsroot, cvsmodule);
	if (!cvs)
		return -1;

	for_each_hash(revision_meta_hash, zero_cvs_revision_util, NULL);

	fprintf(stderr, "validating commit meta by rls\n");

	rc = cvs_rls(cvs, branch_name, 0, timestamp, on_every_file_revision, revision_meta_hash);

	for_each_hash(revision_meta_hash, validate_cvs_revision_util, NULL);
	return rc;
}

static int validate_cvs_branch_meta(const char *branch_name)
{
	struct strbuf ref_sb = STRBUF_INIT;
	struct strbuf meta_ref_sb = STRBUF_INIT;
	struct hash_table *revision_meta_hash = NULL;
	time_t timestamp;
	unsigned char commit_sha1[20];
	int rc;

	strbuf_addf(&ref_sb, "%s%s", get_ref_prefix(), branch_name);
	strbuf_addf(&meta_ref_sb, "%s%s", get_meta_ref_prefix(), branch_name);

	if (get_sha1(ref_sb.buf, commit_sha1))
		die(_("Failed to resolve '%s' as a valid ref."), ref_sb.buf);

	if (!ref_exists(meta_ref_sb.buf))
		die("No metadata for branch %s", branch_name);

	load_revision_meta(commit_sha1, meta_ref_sb.buf, &timestamp, &revision_meta_hash);
	if (!revision_meta_hash)
		die("Cannot load metadata for branch %s", branch_name);

	rc = validate_commit_meta_by_tree(ref_sb.buf, revision_meta_hash);
	if (!rc) {
		if (!timestamp)
			fprintf(stderr, "skipping meta check by rls, no commit timestamp\n");
		else
			rc = validate_commit_meta_by_rls(branch_name, timestamp, revision_meta_hash);
	}
	strbuf_release(&ref_sb);
	strbuf_release(&meta_ref_sb);
	return rc;
}

static int do_command(struct strbuf *line)
{
	const struct input_command_entry *p = input_command_list;
	static struct string_list batchlines = STRING_LIST_INIT_DUP;
	static const struct input_command_entry *batch_cmd;
	/*
	 * commands can be grouped together in a batch.
	 * Batches are ended by \n. If no batch is active the program ends.
	 * During a batch all lines are buffered and passed to the handler function
	 * when the batch is terminated.
	 */
	if (line->len == 0) {
		if (batch_cmd) {
			batch_cmd->batch_fn(&batchlines);
			batch_cmd = NULL;
			string_list_clear(&batchlines, 0);
			return 0;	/* end of the batch, continue reading other commands. */
		}
		return 1;	/* end of command stream, quit */
	}
	if (batch_cmd) {
		if (prefixcmp(line->buf, batch_cmd->name))
			die("Active %s batch interrupted by %s", batch_cmd->name, line->buf);
		/* buffer batch lines */
		string_list_append(&batchlines, line->buf);
		return 0;
	}

	for (p = input_command_list; p->name; p++) {
		if (!prefixcmp(line->buf, p->name) && (strlen(p->name) == line->len ||
				line->buf[strlen(p->name)] == ' ')) {
			if (p->batchable) {
				batch_cmd = p;
				string_list_append(&batchlines, line->buf);
				return 0;
			}
			return p->fn(line->buf);
		}
	}
	die("Unknown command '%s'\n", line->buf);
	return 0;
}

static int parse_cvs_spec(const char *spec)
{
	/*
	 * TODO: make proper parsing
	 */
	char *idx;

	idx = strrchr(spec, ':');
	if (!idx)
		return -1;

	if (!memchr(spec, '/', idx - spec))
		return -1;

	cvsroot = xstrndup(spec, idx - spec);
	cvsmodule = xstrdup(idx + 1);

	fprintf(stderr, "CVSROOT: %s\n", cvsroot);
	fprintf(stderr, "CVSMODULE: %s\n", cvsmodule);

	return 0;
}

int git_cvshelper_config(const char *var, const char *value, void *dummy)
{
	char *str = NULL;

	if (!strcmp(var, "cvshelper.ignoremodechange")) {
		ignore_mode_change = git_config_bool(var, value);
		return 0;
	}
	else if (!strcmp(var, "cvshelper.pushnorefsupdate")) {
		no_refs_update_on_push = git_config_bool(var, value);
		return 0;
	}
	else if (!strcmp(var, "cvshelper.verifyimport")) {
		verify_import = git_config_bool(var, value);
		return 0;
	}
	else if (!strcmp(var, "cvshelper.cvsprototrace")) {
		if (git_config_pathname((const char **)&str, var, value) || !str)
			return 1;

		setenv("GIT_TRACE_CVS_PROTO", str, 0);
		free(str);
		return 0;
	}
	else if (!strcmp(var, "cvshelper.remotehelpertrace")) {
		if (git_config_pathname((const char **)&str, var, value) || !str)
			return 1;

		setenv("GIT_TRACE_CVS_HELPER", str, 0);
		free(str);
		return 0;
	}
	else if (!strcmp(var, "cvshelper.filememorylimit")) {
		fileMemoryLimit = git_config_ulong(var, value);
		return 0;
	}

	return git_default_config(var, value, dummy);
}

static void cvs_branch_list_item_free(void *p, const char *str)
{
	free_cvs_branch(p);
}

int main(int argc, const char **argv)
{
	struct strbuf buf = STRBUF_INIT;
	static struct remote *remote;
	const char *cvs_root_module;

	if (getenv("WAIT_GDB"))
		sleep(5);

	setenv("TZ", "UTC", 1);
	tzset();

	git_extract_argv0_path(argv[0]);
	setup_git_directory();
	if (argc < 2 || argc > 3) {
		usage("git-remote-cvs <remote-name> [<url>]");
		return 1;
	}

	git_config(git_cvshelper_config, NULL);
	remote = remote_get(argv[1]);
	cvs_root_module = (argc == 3) ? argv[2] : remote->url[0];

	if (parse_cvs_spec(cvs_root_module))
		die("Malformed repository specification. "
		    "Should be [:method:][[user][:password]@]hostname[:[port]]/path/to/repository:module/path");

	fprintf(stderr, "git_dir: %s\n", get_git_dir());

	if (!ref_exists("HEAD")) {
		fprintf(stderr, "Initial import!\n");
		initial_import = 1;
	}

	set_ref_prefix_remote(remote->name);
	fprintf(stderr, "ref_prefix %s\n", get_ref_prefix());
	fprintf(stderr, "private_ref_prefix %s\n", get_private_ref_prefix());

	if (find_hook("cvs-import-commit"))
		have_import_hook = 1;
	if (find_hook("cvs-export-commit"))
		have_export_hook = 1;

	if (getenv("NO_REFS_UPDATE_ON_PUSH"))
		no_refs_update_on_push = 1;

	if (getenv("VERIFY_ONLY")) {
		validate_cvs_branch_meta(getenv("VERIFY_ONLY"));
		return 0;
	}

	while (1) {
		if (helper_strbuf_getline(&buf, stdin, '\n') == EOF) {
			if (ferror(stdin))
				die("Error reading command stream");
			else
				die("Unexpected end of command stream");
		}
		if (do_command(&buf))
			break;
		strbuf_reset(&buf);
	}

	string_list_clear_func(&cvs_branch_list, cvs_branch_list_item_free);
	if (cvs) {
		int ret = cvs_terminate(cvs);

		fprintf(stderr, "done, rc=%d\n", ret);
	}

	cvs_authors_store();

	strbuf_release(&buf);
	return 0;
}
