#include "cache.h"
#include "remote.h"
#include "strbuf.h"
#include "url.h"
#include "refs.h"
#include "exec_cmd.h"
#include "run-command.h"
#include "vcs-cvs/client.h"
#include "vcs-cvs/meta.h"
#include "notes.h"
#include "argv-array.h"
#include "commit.h"

/*
 * TODO:
 * - skip dead file addition to HEAD branch
 * - depth
 * - check that metadata correspond to ls-tree files (all files have rev, but no extra)
 * - authors ref/note
 * - safe cancelation point + update time for branch OR ref cmp
 */

static const char trace_key[] = "GIT_TRACE_CVS_HELPER";
/*
 * FIXME:
 */
unsigned long fileMemoryLimit = 2 * 1024 * 1024 * 1024L; //50*1024*1024; /* 50m */

//static int dump_from_file;
//static const char *private_ref;
//static const char *remote_ref = "refs/heads/master";
//static const char *marksfilename, *notes_ref;
struct rev_note { unsigned int rev_nr; };

static int depth = 0;
static int verbosity = 0;
static int progress = 0;
static int followtags = 0;
static int dry_run = 0;
static int initial_import = 0;

struct cvs_transport *cvs = NULL;
struct meta_map *branch_meta_map;
static const char *cvsroot = NULL;
static const char *cvsmodule = NULL;
static struct string_list *import_branch_list = NULL;

#define HASH_TABLE_INIT { 0, 0, NULL }
static struct hash_table cvsauthors_hash = HASH_TABLE_INIT;
static int cvsauthors_hash_modified = 0;
struct cvsauthor {
	char *userid;
	char *ident;
};
static char *cvsauthors_lookup(const char *userid);
static void cvsauthors_add(char *userid, char *ident);
static void cvsauthors_load();

static const char import_commit_edit[] = "IMPORT_COMMIT_EDIT";
static int have_import_hook = 0;

static int cmd_capabilities(const char *line);
static int cmd_option(const char *line);
static int cmd_batch_import(struct string_list *list);
static int cmd_list(const char *line);
static int import_branch_by_name(const char *branch_name);

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
	{ "list", cmd_list, NULL, 0 },
	{ NULL, NULL, NULL, 0 }
};

unsigned int hash_userid(const char *userid)
{
	//unsigned int hash = 0x123;
	unsigned int hash = 0x12375903;

	while (*userid) {
		unsigned char c = *userid++;
		//c = icase_hash(c);
		hash = hash*101 + c;
	}
	return hash;
}

static inline const char *strbuf_hex_unprintable(struct strbuf *sb)
{
	static const char hex[] = "0123456789abcdef";
	struct strbuf out = STRBUF_INIT;
	char *c;

	for (c = sb->buf; c < sb->buf + sb->len; c++) {
		if (isprint(*c)) {
			strbuf_addch(&out, *c);
		}
		else {
			strbuf_addch(&out, '\\');
			strbuf_addch(&out, hex[(unsigned char)*c >> 4]);
			strbuf_addch(&out, hex[*c & 0xf]);
		}
	}

	strbuf_swap(sb, &out);
	strbuf_release(&out);
	return sb->buf;
}

enum direction {
	OUT,
	IN
};

static void remote_helper_proto_trace(const char *buf, size_t len, int direction)
{
	struct strbuf out = STRBUF_INIT;
	struct strbuf **lines, **it;

	if (!trace_want(trace_key))
		return;

	lines = strbuf_split_buf(buf, len, '\n', 0);
	for (it = lines; *it; it++) {
		if (it == lines)
			strbuf_addf(&out, "RHELPER %4zu %s %s\n", len,
				    direction == OUT ? "->" : "<-",
				    strbuf_hex_unprintable(*it));
		else
			strbuf_addf(&out, "RHELPER      %s %s\n",
				    direction == OUT ? "->" : "<-",
				    strbuf_hex_unprintable(*it));
	}
	strbuf_list_free(lines);

	trace_strbuf(trace_key, &out);
	strbuf_release(&out);
}

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
		remote_helper_proto_trace(tracebuf.buf, tracebuf.len, OUT);
		strbuf_release(&tracebuf);
	}


	return written;
}

static ssize_t helper_write(const char *buf, size_t len)
{
	ssize_t written;
	if (trace_want(trace_key))
		remote_helper_proto_trace(buf, len, OUT);

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

	if (trace_want(trace_key)) {
		remote_helper_proto_trace(sb->buf, sb->len, IN);
	}

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
	helper_printf("push\n");
	helper_printf("option\n");
	//helper_printf("refspec refs/heads/*:%s*\n\n", ref_prefix);
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

static void terminate_batch(void)
{
	/* terminate a current batch's fast-import stream */
	helper_printf("done\n");
	helper_flush();
}

/* NOTE: 'ref' refers to a git reference, while 'rev' refers to a svn revision. */
/*static char *read_ref_note(const unsigned char sha1[20])
{
	const unsigned char *note_sha1;
	char *msg = NULL;
	unsigned long msglen;
	enum object_type type;

	init_notes(NULL, notes_ref, NULL, 0);
	if (!(note_sha1 = get_note(NULL, sha1)))
		return NULL;	// note tree not found
	if (!(msg = read_sha1_file(note_sha1, &type, &msglen)))
		error("Empty notes tree. %s", notes_ref);
	else if (!msglen || type != OBJ_BLOB) {
		error("Note contains unusable content. "
			"Is something else using this notes tree? %s", notes_ref);
		free(msg);
		msg = NULL;
	}
	free_notes(NULL);
	return msg;
}

static int parse_rev_note(const char *msg, struct rev_note *res)
{
	const char *key, *value, *end;
	size_t len;

	while (*msg) {
		end = strchr(msg, '\n');
		len = end ? end - msg : strlen(msg);

		key = "Revision-number: ";
		if (!prefixcmp(msg, key)) {
			long i;
			char *end;
			value = msg + strlen(key);
			i = strtol(value, &end, 0);
			if (end == value || i < 0 || i > UINT32_MAX)
				return -1;
			res->rev_nr = i;
			return 0;
		}
		msg += len + 1;
	}
	// didn't find it
	return -1;
}

static int note2mark_cb(const unsigned char *object_sha1,
		const unsigned char *note_sha1, char *note_path,
		void *cb_data)
{
	FILE *file = (FILE *)cb_data;
	char *msg;
	unsigned long msglen;
	enum object_type type;
	struct rev_note note;

	if (!(msg = read_sha1_file(note_sha1, &type, &msglen)) ||
			!msglen || type != OBJ_BLOB) {
		free(msg);
		return 1;
	}
	if (parse_rev_note(msg, &note))
		return 2;
	if (fprintf(file, ":%d %s\n", note.rev_nr, sha1_to_hex(object_sha1)) < 1)
		return 3;
	return 0;
}

static void regenerate_marks(void)
{
	int ret;
	FILE *marksfile = fopen(marksfilename, "w+");

	if (!marksfile)
		die_errno("Couldn't create mark file %s.", marksfilename);
	ret = for_each_note(NULL, 0, note2mark_cb, marksfile);
	if (ret)
		die("Regeneration of marks failed, returned %d.", ret);
	fclose(marksfile);
}

static void check_or_regenerate_marks(int latestrev)
{
	FILE *marksfile;
	struct strbuf sb = STRBUF_INIT;
	struct strbuf line = STRBUF_INIT;
	int found = 0;

	if (latestrev < 1)
		return;

	init_notes(NULL, notes_ref, NULL, 0);
	marksfile = fopen(marksfilename, "r");
	if (!marksfile) {
		regenerate_marks();
		marksfile = fopen(marksfilename, "r");
		if (!marksfile)
			die_errno("cannot read marks file %s!", marksfilename);
		fclose(marksfile);
	} else {
		strbuf_addf(&sb, ":%d ", latestrev);
		while (strbuf_getline(&line, marksfile, '\n') != EOF) {
			if (!prefixcmp(line.buf, sb.buf)) {
				found++;
				break;
			}
		}
		fclose(marksfile);
		if (!found)
			regenerate_marks();
	}
	free_notes(NULL);
	strbuf_release(&sb);
	strbuf_release(&line);
}*/

static void unixtime_to_date(time_t timestamp, struct strbuf *date)
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

static int print_revision(void *ptr, void *data)
{
	struct file_revision *rev = ptr;

	if (rev->prev) {
		struct file_revision *prev = rev->prev;
		while (prev && prev->ismerged && prev->prev)
			prev = prev->prev;
		if (prev->ismerged)
			fprintf(stderr, "\tunknown->%s-", rev->prev->revision);
		else
		fprintf(stderr, "\t%s->", rev->prev->revision);
	}
	else {
		fprintf(stderr, "\tunknown->");
	}
	fprintf(stderr, "%s\t%s", rev->revision, rev->path);

	if (rev->isdead)
		fprintf(stderr, " (dead)\n");
	else
		fprintf(stderr, "\n");
	return 0;
}

static struct strbuf sb1 = STRBUF_INIT;
static struct strbuf sb2 = STRBUF_INIT;
static struct strbuf sb3 = STRBUF_INIT;
static void print_ps(struct cvs_transport *cvs, struct patchset *ps)
{
	unixtime_to_date(ps->timestamp, &sb1);
	unixtime_to_date(ps->timestamp_last, &sb2);
	unixtime_to_date(ps->cancellation_point, &sb3);

	fprintf(stderr, "%s\n"
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

static int commit_revision(void *ptr, void *data)
{
	static struct cvsfile file = CVSFILE_INIT;
	struct file_revision *rev = ptr;
	int rc;

	fprintf(stderr, "commit_revision %s %s\n", rev->path, rev->revision);

	if (rev->isdead) {
		helper_printf("D %s\n", rev->path);
		return 0;
	}

	rc = cvs_checkout_rev(cvs, rev->path, rev->revision, &file);
	if (rc == -1)
		die("Cannot checkout file %s rev %s", rev->path, rev->revision);

	if (file.isdead) {
		helper_printf("D %s\n", rev->path);
		return 0;
	}

	//helper_printf("M 100%.3o %s %s\n", hash, rev->path);
	helper_printf("M 100%.3o inline %s\n", file.isexec ? 0755 : 0644, rev->path);
	helper_printf("data %zu\n", file.file.len);
	helper_write(file.file.buf, file.file.len);
	helper_printf("\n");

	return 0;
}

static int run_author_convert_hook(const char *userid, struct strbuf *author_ident)
{
	struct child_process proc;
	const char *argv[3];
	int code;

	argv[0] = find_hook("cvs-author-convert");
	if (!argv[0])
		return 0;

	argv[1] = userid;
	argv[2] = NULL;

	memset(&proc, 0, sizeof(proc));
	proc.argv = argv;
	proc.out = -1;

	code = start_command(&proc);
	if (code)
		return code;

	strbuf_getwholeline_fd(author_ident, proc.out, '\n');
	strbuf_trim(author_ident);
	close(proc.out);
	return finish_command(&proc);
}

static char *author_convert_via_hook(const char *userid)
{
	struct strbuf author_ident = STRBUF_INIT;

	if (run_author_convert_hook(userid, &author_ident))
		return NULL;

	if (!author_ident.len) {
		strbuf_addf(&author_ident, "%s <unknown>", userid);
		return strbuf_detach(&author_ident, NULL);
	}

	/*
	 * TODO: proper verify
	 */
	const char *lt;
	const char *gt;

	lt = index(author_ident.buf, '<');
	gt = index(author_ident.buf, '>');

	if (!lt && !gt)
		strbuf_addstr(&author_ident, " <unknown>");

	return strbuf_detach(&author_ident, NULL);
}

static const char *author_convert(const char *userid)
{
	char *ident;

	ident = cvsauthors_lookup(userid);
	if (ident)
		return ident;

	ident = author_convert_via_hook(userid);
	if (ident)
		cvsauthors_add(xstrdup(userid), ident);

	return ident;
}

static int markid = 0;
static int commit_patchset(struct patchset *ps, const char *branch_name, struct strbuf *parent_mark)
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

		helper_printf("commit %s%s\n", get_ref_prefix(), branch_name);
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
		helper_printf("commit %s%s\n", get_ref_prefix(), branch_name);
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
	for_each_hash(ps->revision_hash, commit_revision, NULL);
	//helper_printf("\n");
	helper_flush();

	return markid;
}

static int commit_meta_revision(void *ptr, void *data)
{
	struct file_revision *rev = ptr;

	if (!rev->isdead)
		helper_printf("%s:%s\n", rev->revision, rev->path);
	//helper_printf("%s:%c:%s\n", rev->revision, rev->isdead ? '-' : '+', rev->path);
	return 0;
}

static int print_revision_changes(void *ptr, void *data)
{
	struct file_revision *rev = ptr;

	if (!rev->isdead)
		helper_printf("updated %s %s\n", rev->revision, rev->path);
	else
		helper_printf("deleted %s %s\n", rev->revision, rev->path);
	//helper_printf("%s:%c:%s\n", rev->revision, rev->isdead ? '-' : '+', rev->path);
	return 0;
}

static int commit_meta(struct hash_table *meta, struct patchset *ps, const char *branch_name, struct strbuf *commit_mark, struct strbuf *parent_mark)
{
	markid++;
	helper_printf("commit %s%s\n", get_meta_ref_prefix(), branch_name);
	helper_printf("mark :%d\n", markid);
	//helper_printf("author %s <%s> %ld +0000\n", ps->author, "unknown", ps->timestamp);
	helper_printf("committer %s <%s> %ld +0000\n", ps->author, "unknown", ps->timestamp_last);
	helper_printf("data <<EOM\n");
	helper_printf("cvs meta update\n");
	for_each_hash(ps->revision_hash, print_revision_changes, NULL);
	helper_printf("EOM\n");
	if (parent_mark->len)
		helper_printf("from %s\n", parent_mark->buf);
	helper_printf("N inline %s\n", commit_mark->buf);
	helper_printf("data <<EON\n");
	if (ps->cancellation_point)
		helper_printf("UPDATE:%ld\n", ps->cancellation_point);
	helper_printf("--\n");
	for_each_hash(meta, commit_meta_revision, NULL);
	helper_printf("EON\n");
	//helper_printf("\n");
	helper_flush();

	return markid;
}

static int commit_blob(void *buf, size_t size)
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
	struct file_revision *rev = ptr;
	struct hash_table *meta = data;

	unsigned int hash;
	void **pos;

	hash = hash_path(rev->path);
	pos = insert_hash(hash, rev, meta);
	if (pos) {
		struct file_revision *prev = *pos;

		if (strcmp(rev->path, prev->path))
			die("file path hash collision");

		*pos = rev;
	}
	return 0;
}


/*struct cvsfile {
	struct strbuf path;
	struct strbuf revision;
	int mode;
	int isdead:1;
	struct strbuf file;
};
*/
void on_file_checkout(struct cvsfile *file, void *data)
{
	struct hash_table *meta_revision_hash = data;
	int mark;

	/*
	 * FIXME: support files on disk
	 */
	if (!file->ismem)
		die("no support for files on disk yet");

	mark = commit_blob(file->file.buf, file->file.len);

	add_file_revision_meta_hash(meta_revision_hash, file->path.buf, file->revision.buf, file->isdead, file->isexec, mark);
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

	rc = cvs_checkout_branch(cvs_co, branch_name, import_time, on_file_checkout, meta_revision_hash);
	if (rc)
		die("cvs checkout of %s date %ld failed", branch_name, import_time);

	return cvs_terminate(cvs_co);
}

int count_dots(const char *rev)
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

static char *get_rev_branch(struct file_revision_meta *file_meta)
{
	return cvs_get_rev_branch(cvs, file_meta->path, file_meta->revision);
}

struct find_rev_data {
	struct file_revision_meta *file_meta;
	int dots;
};
static int find_longest_rev(void *ptr, void *data)
{
	struct file_revision_meta *rev_meta = ptr;
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

	return get_rev_branch(find_rev_data.file_meta);
}

static int compare_commit_meta(unsigned char sha1[20], const char *meta_ref, struct hash_table *meta_revision_hash)
{
	struct file_revision_meta *file_meta;
	char *buf;
	char *p;
	char *revision;
	char *path;
	unsigned long size;
	int rev_mismatches = 0;

	buf = read_note_of(sha1, meta_ref, &size);
	if (!buf)
		return -1;

	p = buf;
	while ((p = parse_meta_line(buf, size, &revision, &path, p))) {
		if (strcmp(revision, "--") == 0)
			break;
	}

	while ((p = parse_meta_line(buf,size, &revision, &path, p))) {
		file_meta = lookup_hash(hash_path(path), meta_revision_hash);
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
	struct strbuf branch_meta_ref = STRBUF_INIT;
	const char *commit_ref = NULL;
	int rev_mismatches_min = INT_MAX;
	int rev_mismatches;

	save_commit_buffer = 0;

	strbuf_addf(&branch_ref, "%s%s", get_ref_prefix(), parent_branch_name);
	strbuf_addf(&branch_meta_ref, "%s%s", get_meta_ref_prefix(), parent_branch_name);

	if (get_sha1_commit(branch_ref.buf, sha1))
		die("cannot find last commit on branch ref %s", branch_ref.buf);

	commit = lookup_commit(sha1);

	for (;;) {
		if (parse_commit(commit))
			die("cannot parse commit %s", sha1_to_hex(sha1));

		if (commit->date <= time) {
			rev_mismatches = compare_commit_meta(commit->object.sha1, branch_meta_ref.buf, meta_revision_hash);
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
	strbuf_release(&branch_meta_ref);
	return commit_ref;
}

static int commit_revision_by_mark(void *ptr, void *data)
{
	struct file_revision_meta *rev = ptr;

	helper_printf("M 100%.3o :%d %s\n", rev->isexec ? 0755 : 0644, rev->mark, rev->path);
	//helper_printf("\n");

	return 0;
}

static int commit_branch_initial(struct hash_table *meta_revision_hash,
		const char *branch_name, time_t date, const char *parent_commit_ref,
		const char *parent_branch_name)
{
	markid++;

	helper_printf("commit %s%s\n", get_ref_prefix(), branch_name);
	helper_printf("mark :%d\n", markid);
	helper_printf("author git-remote-cvs <none> %ld +0000\n", date);
	helper_printf("committer git-remote-cvs <none> %ld +0000\n", date);
	helper_printf("data <<EON\n");
	helper_printf("initial import of branch: %s\nparent branch: %s\n", branch_name, parent_branch_name);
	helper_printf("EON\n");
	//helper_printf("merge %s\n", parent_commit_ref);
	helper_printf("from %s\n", parent_commit_ref);
	helper_printf("deleteall\n");
	for_each_hash(meta_revision_hash, commit_revision_by_mark, NULL);
	helper_flush();

	return markid;
}
static int import_branch(const char *branch_name, struct branch_meta *branch_meta)
{
	int rc;
	int mark;
	char *parent_branch_name;
	struct string_list_item *item;
	const char *parent_commit;
	time_t import_time = find_first_commit_time(branch_meta);
	if (!import_time)
		die("import time is 0");
	import_time--;
	fprintf(stderr, "import time is %s\n", show_date(import_time, 0, DATE_RFC2822));

	rc = checkout_branch(branch_name, import_time, branch_meta->last_commit_revision_hash);
	if (rc == -1)
		die("initial branch checkout failed %s", branch_name);

	if (is_empty_hash(branch_meta->last_commit_revision_hash))
		return 0;

	parent_branch_name = find_parent_branch(branch_name, branch_meta->last_commit_revision_hash);
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
						branch_meta->last_commit_revision_hash);
	fprintf(stderr, "PARENT COMMIT: %s\n", parent_commit);

	mark = commit_branch_initial(branch_meta->last_commit_revision_hash,
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
	struct strbuf commit_mark_sb = STRBUF_INIT;
	struct strbuf meta_mark_sb = STRBUF_INIT;
	struct strbuf branch_ref = STRBUF_INIT;
	struct hash_table meta_revision_hash;

	fprintf(stderr, "importing CVS branch %s\n", branch_name);

	struct branch_meta *branch_meta;
	int psnum = 0;

	strbuf_addf(&branch_ref, "%s%s", get_meta_ref_prefix(), branch_name);

	/*
	 * FIXME
	 */
	//if (!strcmp(branch_name, "master"))
	//	branch_meta = meta_map_find(branch_meta_map, "HEAD");
	//else
		branch_meta = meta_map_find(branch_meta_map, branch_name);

	if (!branch_meta && !ref_exists(branch_ref.buf))
		die("Cannot find meta for branch %s\n", branch_name);

	if (is_empty_hash(branch_meta->last_commit_revision_hash) &&
	    strcmp(branch_name, "HEAD")) {
		/*
		 * no meta, do cvs checkout
		 */
		mark = import_branch(branch_name, branch_meta);
		if (mark == -1)
			die("import_branch failed %s", branch_name);
		if (mark > 0)
			strbuf_addf(&commit_mark_sb, ":%d", mark);
	}
	aggregate_patchsets(branch_meta);

	init_hash(&meta_revision_hash);
	merge_revision_hash(&meta_revision_hash, branch_meta->last_commit_revision_hash);

	cvsauthors_load();

	struct patchset *ps = branch_meta->patchset_list->head;
	while (ps) {
		psnum++;
		fprintf(stderr, "-->>------------------\n");
		print_ps(cvs, ps);
		fprintf(stderr, "--<<------------------\n\n");

		mark = commit_patchset(ps, branch_name, &commit_mark_sb);
		strbuf_reset(&commit_mark_sb);
		strbuf_addf(&commit_mark_sb, ":%d", mark);

		merge_revision_hash(&meta_revision_hash, ps->revision_hash);
		mark = commit_meta(&meta_revision_hash, ps, branch_name, &commit_mark_sb, &meta_mark_sb);
		strbuf_reset(&meta_mark_sb);
		strbuf_addf(&meta_mark_sb, ":%d", mark);

		ps = ps->next;
	}
	if (psnum) {
		fprintf(stderr, "Branch: %s Commits number: %d\n", branch_name, psnum);

		helper_printf("checkpoint\n");
		helper_flush();

		if (initial_import && !strcmp(branch_name, "HEAD")) {
			helper_printf("reset HEAD\n");
			helper_printf("from %s\n", commit_mark_sb.buf);
		}
	}
	else {
		fprintf(stderr, "Branch: %s is up to date\n", branch_name);
	}

	strbuf_release(&commit_mark_sb);
	strbuf_release(&meta_mark_sb);
	strbuf_release(&branch_ref);
	free_hash(&meta_revision_hash);

/*	int code;
	int dumpin_fd;
	char *note_msg;
	unsigned char head_sha1[20];
	unsigned int startrev;
	struct argv_array svndump_argv = ARGV_ARRAY_INIT;
	struct child_process svndump_proc;

	if (read_ref(private_ref, head_sha1))
		startrev = 0;
	else {
		note_msg = read_ref_note(head_sha1);
		if(note_msg == NULL) {
			warning("No note found for %s.", private_ref);
			startrev = 0;
		} else {
			struct rev_note note = { 0 };
			if (parse_rev_note(note_msg, &note))
				die("Revision number couldn't be parsed from note.");
			startrev = note.rev_nr + 1;
			free(note_msg);
		}
	}
	check_or_regenerate_marks(startrev - 1);

	if (dump_from_file) {
		dumpin_fd = open(url, O_RDONLY);
		if(dumpin_fd < 0)
			die_errno("Couldn't open svn dump file %s.", url);
	} else {
		memset(&svndump_proc, 0, sizeof(struct child_process));
		svndump_proc.out = -1;
		argv_array_push(&svndump_argv, "svnrdump");
		argv_array_push(&svndump_argv, "dump");
		argv_array_push(&svndump_argv, url);
		argv_array_pushf(&svndump_argv, "-r%u:HEAD", startrev);
		svndump_proc.argv = svndump_argv.argv;

		code = start_command(&svndump_proc);
		if (code)
			die("Unable to start %s, code %d", svndump_proc.argv[0], code);
		dumpin_fd = svndump_proc.out;
	}
	helper_printf("feature import-marks-if-exists=%s\n"
			"feature export-marks=%s\n", marksfilename, marksfilename);

	svndump_init_fd(dumpin_fd, STDIN_FILENO);
	svndump_read(url, private_ref, notes_ref);
	svndump_deinit();
	svndump_reset();

	close(dumpin_fd);
	if (!dump_from_file) {
		code = finish_command(&svndump_proc);
		if (code)
			warning("%s, returned %d", svndump_proc.argv[0], code);
		argv_array_clear(&svndump_argv);
	}
*/
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

	return 0;
}

static int called = 0;
static int skipped = 0;
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

	if (meta->last_revision_timestamp < timestamp)
		add_file_revision(meta, path, revision, author, msg, timestamp, isdead);
	else
		skipped++;
	called++;
}

static time_t update_since = 0;
int on_each_ref(const char *branch, const unsigned char *sha1, int flags, void *data)
{
	struct meta_map *branch_meta_map = data;
	struct branch_meta *meta;

	meta = meta_map_find(branch_meta_map, branch);
	if (!meta) {
		meta = new_branch_meta(branch);
		meta_map_add(branch_meta_map, branch, meta);

		//helper_printf("? refs/heads/%s\n", branch);
	}

	if (!update_since ||
	    update_since > meta->last_revision_timestamp)
		update_since = meta->last_revision_timestamp;
	return 0;
}

static int cmd_list(const char *line)
{
	cvs = cvs_connect(cvsroot, cvsmodule);
	if (!cvs)
		return -1;

	fprintf(stderr, "connected to cvs server\n");

	struct meta_map_entry *branch_meta;

	if (initial_import) {
		cvs_rlog(cvs, 0, 0, add_file_revision_cb, branch_meta_map);
		fprintf(stderr, "CB CALLED: %d\n", called);
		fprintf(stderr, "SKIPPED: %d\n", skipped);

		for_each_branch_meta(branch_meta, branch_meta_map) {
			//aggregate_patchsets(branch_meta->meta);
			//if (branch_meta->meta->patchset_list->head)
			if (branch_meta->meta->rev_list->size)
				helper_printf("? refs/heads/%s\n", branch_meta->branch_name);
			/*
			 * FIXME:
			 */
			//if (!strcmp(branch_meta->branch_name, "HEAD"))
			//	helper_printf("? refs/heads/master\n");
			//else
			//	helper_printf("? refs/heads/%s\n", branch_meta->branch_name);
		}
		helper_printf("\n");
	}
	else {
		for_each_ref_in(get_meta_ref_prefix(), on_each_ref, branch_meta_map);
		fprintf(stderr, "update since: %ld\n", update_since);

		/*
		 * FIXME: we'll skip branches which were not imported during
		 * initial import. If some changes for these branches arrives
		 * and import will pick it up this time, then history will be
		 * truncated. Have to detect this case and do full rlog for
		 * importing such branches.
		 */
		cvs_rlog(cvs, update_since, 0, add_file_revision_cb, branch_meta_map);
		fprintf(stderr, "CB CALLED: %d\n", called);
		fprintf(stderr, "SKIPPED: %d\n", skipped);

		/*
		 * FIXME: try to print only branches that have changes
		 */
		for_each_branch_meta(branch_meta, branch_meta_map) {
			//aggregate_patchsets(branch_meta->meta);
			//if (branch_meta->meta->patchset_list->head)
			if (branch_meta->meta->rev_list->size)
				helper_printf("? refs/heads/%s\n", branch_meta->branch_name);
			else
				fprintf(stderr, "Branch: %s is up to date\n", branch_meta->branch_name);
			//helper_printf("? refs/heads/%s\n", branch_meta->branch_name);
		}

		//for_each_ref_in(get_meta_ref_prefix(), on_each_ref, branch_meta_map);
		helper_printf("\n");
	}
	helper_flush();
	return 0;
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
			//struct string_list_item *item;
			//for_each_string_list_item(item, &batchlines)
			//	batch_cmd->fn(item->string);
			batch_cmd->batch_fn(&batchlines);
			terminate_batch();
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

static char *cvsauthors_lookup(const char *userid)
{
	struct cvsauthor *auth;

	auth = lookup_hash(hash_userid(userid), &cvsauthors_hash);
	if (auth)
		return auth->ident;

	return NULL;
}

static void cvsauthors_add(char *userid, char *ident)
{
	struct cvsauthor *auth;
	struct cvsauthor **ppauth;

	auth = xmalloc(sizeof(*auth));
	auth->userid = userid;
	auth->ident = ident;

	ppauth = (struct cvsauthor **)insert_hash(hash_userid(userid), auth, &cvsauthors_hash);
	if (ppauth) {
		if (strcmp((*ppauth)->userid, auth->userid))
			error("cvs-authors userid hash colision %s %s", (*ppauth)->userid, auth->userid);
		else
			error("cvs-authors userid dup %s", auth->userid);
	}
	cvsauthors_hash_modified = 1;
}

static void cvsauthors_load()
{
	struct strbuf line = STRBUF_INIT;
	char *p;
	FILE *fd;

	if (!is_empty_hash(&cvsauthors_hash))
		return;

	fd = fopen(git_path("cvs-authors"), "r");
	if (!fd)
		return;

	while (!strbuf_getline(&line, fd, '\n')) {
		p = strchr(line.buf, '=');
		if (!p) {
			warning("bad formatted cvs-authors line: %s", line.buf);
			continue;
		}
		*p++ = '\0';

		cvsauthors_add(xstrdup(line.buf), xstrdup(p));
	}

	fclose(fd);
	strbuf_release(&line);
	cvsauthors_hash_modified = 0;
}

static int cvsauthor_item_store(void *ptr, void *data)
{
	struct cvsauthor *auth = ptr;
	FILE *fd = data;

	fprintf(fd, "%s=%s\n", auth->userid, auth->ident);
	free(auth->userid);
	free(auth->ident);
	free(auth);
	return 0;
}

static void cvsauthors_store()
{
	if (!cvsauthors_hash_modified)
		return;

	FILE *fd = fopen(git_path("cvs-authors"), "w");
	if (!fd)
		return;

	for_each_hash(&cvsauthors_hash, cvsauthor_item_store, fd);
	fclose(fd);
	free_hash(&cvsauthors_hash);
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

int main(int argc, const char **argv)
{
	struct strbuf buf = STRBUF_INIT, url_sb = STRBUF_INIT,
			private_ref_sb = STRBUF_INIT;
	struct strbuf ref_prefix_sb = STRBUF_INIT;
	static struct remote *remote;
	const char *cvs_root_module;

	//sleep(5);

	setenv("TZ", "UTC", 1);
	tzset();

	git_extract_argv0_path(argv[0]);
	setup_git_directory();
	if (argc < 2 || argc > 3) {
		usage("git-remote-cvs <remote-name> [<url>]");
		return 1;
	}

	remote = remote_get(argv[1]);
	cvs_root_module = (argc == 3) ? argv[2] : remote->url[0];

	if (parse_cvs_spec(cvs_root_module))
		die("Malformed repository specification. "
		    "Should be [:method:][[user][:password]@]hostname[:[port]]/path/to/repository:module/path");

	//if (!prefixcmp(url_in, "file://")) {
	//	dump_from_file = 1;
	//	url = url_decode(url_in + sizeof("file://")-1);
	//} else {
	//	dump_from_file = 0;
	//	end_url_with_slash(&url_sb, url_in);
	//	url = url_sb.buf;
	//}

	//strbuf_addf(&private_ref_sb, "refs/svn/%s/master", remote->name);
	//private_ref = private_ref_sb.buf;

	//strbuf_addf(&notes_ref_sb, "refs/notes/%s/revs", remote->name);
	//notes_ref = notes_ref_sb.buf;

	//strbuf_addf(&marksfilename_sb, "%s/info/fast-import/remote-svn/%s.marks",
	//	get_git_dir(), remote->name);
	//marksfilename = marksfilename_sb.buf;

	fprintf(stderr, "git_dir: %s\n", get_git_dir());

	/*char **pe = environ;
	while(*pe) {
		fprintf(stderr, "env: %s\n", *pe);
		pe++;
	}*/

	if (!ref_exists("HEAD")) {
		fprintf(stderr, "Initial import!\n");
		initial_import = 1;
	}

	/*int i;
	for (i = 0; i < remote->fetch_refspec_nr; i++) {
		fprintf(stderr, "refspec to fetch to: %s\n", remote->fetch->dst);
		if (strstr(remote->fetch->dst, "remote"))
			is_bare = 0;
	}*/

	if (is_bare_repository()) {
		strbuf_addstr(&ref_prefix_sb, "refs/heads/");
		//strbuf_addstr(&ref_prefix_sb, "refs/cvs/heads/");
	}
	else {
		strbuf_addf(&ref_prefix_sb, "refs/remotes/%s/", remote->name);
		//strbuf_addf(&ref_prefix_sb, "refs/cvs/remotes/%s/", remote->name);
	}
	set_ref_prefix(ref_prefix_sb.buf);
	fprintf(stderr, "%s\n", get_ref_prefix());

	branch_meta_map = xmalloc(sizeof(*branch_meta_map));
	meta_map_init(branch_meta_map);

	if (find_hook("cvs-import-commit"))
		have_import_hook = 1;

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

	meta_map_release(branch_meta_map);
	if (cvs) {
		int ret = cvs_terminate(cvs);

		fprintf(stderr, "done, rc=%d\n", ret);
	}

	/*if (ref_exists("refs/remotes/cvs/HEAD")) {
		struct pretty_print_context pctx = {0};
		struct strbuf author_ident = STRBUF_INIT;
		struct strbuf committer_ident = STRBUF_INIT;
		unsigned char sha1[20];

		if (get_sha1_commit("refs/remotes/cvs/HEAD" "c71ad2674f80b384", sha1))
			die("cannot find commit");
		struct commit *cm;
		cm = lookup_commit(sha1);
		if (parse_commit(cm))
			die("cannot parse commit");

		format_commit_message(cm, "%an <%ae>", &author_ident, &pctx);
		format_commit_message(cm, "%cn <%ce>", &committer_ident, &pctx);
		sleep(5);
	}*/

	cvsauthors_store();

	strbuf_release(&buf);
	strbuf_release(&url_sb);
	strbuf_release(&private_ref_sb);
	strbuf_release(&ref_prefix_sb);
	return 0;
}
