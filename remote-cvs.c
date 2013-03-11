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

/*
 * TODO:
 * - depth
 * - check that metadata correspond to ls-tree files (all files have rev, but no extra)
 * - authors hook
 * - msg rewrite hook
 * - authors ref/note
 * - branch parents
 * - safe cancelation point + update time for branch OR ref cmp
 */

static const char trace_key[] = "GIT_TRACE_REMOTE_HELPER_PROTO";
unsigned long fileMemoryLimit = 50*1024*1024; /* 50m */

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

static int cmd_capabilities(const char *line);
static int cmd_option(const char *line);
static int cmd_import(const char *line);
static int cmd_list(const char *line);

typedef int (*input_command_handler)(const char *);
struct input_command_entry {
	const char *name;
	input_command_handler fn;
	unsigned char batchable;	/* whether the command starts or is part of a batch */
};

static const struct input_command_entry input_command_list[] = {
	{ "capabilities", cmd_capabilities, 0 },
	{ "option", cmd_option, 0 },
	{ "import", cmd_import, 1 },
	{ "list", cmd_list, 0 },
	{ NULL, NULL }
};

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
	outgoing,
	incomming,
};

static void remote_helper_proto_trace(const char *buf, size_t len, enum direction dir)
{
	struct strbuf out = STRBUF_INIT;
	struct strbuf **lines, **it;

	if (!trace_want(trace_key))
		return;

	lines = strbuf_split_buf(buf, len, '\n', 0);
	for (it = lines; *it; it++) {
		if (it == lines)
			strbuf_addf(&out, "RHELPER %4zu %s %s\n", len,
			            dir == outgoing ? "->" : "<-",
				    strbuf_hex_unprintable(*it));
		else
			strbuf_addf(&out, "RHELPER      %s %s\n",
			            dir == outgoing ? "->" : "<-",
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
		remote_helper_proto_trace(tracebuf.buf, tracebuf.len, outgoing);
		strbuf_release(&tracebuf);
	}


	return written;
}

static ssize_t helper_write(const char *buf, size_t len)
{
	ssize_t written;
	if (trace_want(trace_key))
		remote_helper_proto_trace(buf, len, outgoing);

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
		remote_helper_proto_trace(sb->buf, sb->len, incomming);
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
	helper_printf("export\n");
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
	int mode;

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
	if (file.mode & 0100)
		mode = 0755;
	else
		mode = 0644;
	helper_printf("M 100%.3o inline %s\n", mode, rev->path);
	helper_printf("data %zu\n", file.file.len);
	helper_write(file.file.buf, file.file.len);
	helper_printf("\n");

	return 0;
}

static int markid = 0;
static int commit_patchset(struct patchset *ps, const char *branch_name, struct strbuf *parent_mark)
{
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

	helper_printf("commit %s%s\n", get_ref_prefix(), branch_name);
	helper_printf("mark :%d\n", markid);
	helper_printf("author %s <%s> %ld +0000\n", ps->author, "unknown", ps->timestamp);
	helper_printf("committer %s <%s> %ld +0000\n", ps->author, "unknown", ps->timestamp_last);
	helper_printf("data %zu\n", strlen(ps->msg));
	helper_printf("%s\n", ps->msg);
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

	helper_printf("%s:%c:%s\n", rev->revision, rev->isdead ? '-' : '+', rev->path);
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
	helper_printf("cvs meta\n");
	helper_printf("EOM\n");
	if (parent_mark->len)
		helper_printf("from %s\n", parent_mark->buf);
	helper_printf("N inline %s\n", commit_mark->buf);
	helper_printf("data <<EON\n");
	helper_printf("--\n");
	for_each_hash(meta, commit_meta_revision, NULL);
	helper_printf("EON\n");
	//helper_printf("\n");
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

static void merge_revision_hash(struct hash_table *meta, struct hash_table *update)
{
	for_each_hash(update, update_revision_hash, meta);
}

static int cmd_import(const char *line)
{
	const char *branch_name;
	static int mark;
	struct strbuf commit_mark_sb = STRBUF_INIT;
	struct strbuf meta_mark_sb = STRBUF_INIT;
	struct strbuf branch_ref = STRBUF_INIT;
	struct hash_table meta_revision_hash;

	if (!(branch_name = gettext_after(line, "import refs/heads/")))
		die("Malformed import command (wrong ref prefix)");

	fprintf(stderr, "importing CVS branch %s\n", branch_name);

	struct branch_meta *branch_meta;
	int psnum = 0;

	strbuf_addf(&branch_ref, "%s%s", get_meta_ref_prefix(), branch_name);

	branch_meta = meta_map_find(branch_meta_map, branch_name);
	if (!branch_meta && !ref_exists(branch_ref.buf))
		die("Cannot find meta for branch %s\n", branch_name);

	aggregate_patchsets(branch_meta);

	init_hash(&meta_revision_hash);
	merge_revision_hash(&meta_revision_hash, branch_meta->last_commit_revision_hash);

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
	fprintf(stderr, "Branch: %s Commits number: %d\n", branch_name, psnum);

	strbuf_release(&commit_mark_sb);
	strbuf_release(&meta_mark_sb);

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

	if (meta->last_revision < timestamp)
		add_file_revision(meta, path, revision, author, msg, timestamp, isdead);
	called++;
}

int on_each_ref(const char *refname, const unsigned char *sha1, int flags, void *cb_data)
{
	struct meta_map *branch_meta_map = cb_data;

	if (!meta_map_find(branch_meta_map, refname));
		helper_printf("? refs/heads/%s\n", refname);
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

		for_each_branch_meta(branch_meta, branch_meta_map) {
			helper_printf("? refs/heads/%s\n", branch_meta->branch_name);
		}
		helper_printf("\n");
	}
	else {
		cvs_rlog(cvs, 0, 0, add_file_revision_cb, branch_meta_map);
		fprintf(stderr, "CB CALLED: %d\n", called);

		for_each_branch_meta(branch_meta, branch_meta_map) {
			helper_printf("? refs/heads/%s\n", branch_meta->branch_name);
		}

		for_each_ref_in(get_meta_ref_prefix(), on_each_ref, branch_meta_map);
		//helper_printf("? %s\n\n", remote_ref);
		//helper_printf("? %s\n\n", "refs/heads/HEAD");
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
			struct string_list_item *item;
			for_each_string_list_item(item, &batchlines)
				batch_cmd->fn(item->string);
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

	strbuf_release(&buf);
	strbuf_release(&url_sb);
	strbuf_release(&private_ref_sb);
	return 0;
}
