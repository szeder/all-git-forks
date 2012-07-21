
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cache.h"
#include "remote.h"
#include "strbuf.h"
#include "url.h"
#include "exec_cmd.h"
#include "run-command.h"
#include "svndump.h"
#include "notes.h"
#include "argv-array.h"

static int debug = 0;

static inline void printd(const char* fmt, ...)
{
	if(debug) {
		va_list vargs;
		va_start(vargs, fmt);
		fprintf(stderr, "rhsvn debug: ");
		vfprintf(stderr, fmt, vargs);
		fprintf(stderr, "\n");
		va_end(vargs);
	}
}

static const char* url;
static int dump_from_file;
static const char *private_ref;
static const char *remote_ref = "refs/heads/master";
static const char *notes_ref;
struct rev_note { unsigned int rev_nr; };

enum cmd_result cmd_capabilities(struct strbuf* line);
enum cmd_result cmd_import(struct strbuf* line);
enum cmd_result cmd_list(struct strbuf* line);
enum cmd_result cmd_terminate(struct strbuf* line);

enum cmd_result { SUCCESS, NOT_HANDLED, TERMINATE };
typedef enum cmd_result (*command)(struct strbuf*);

const command command_list[] = {
		cmd_capabilities, cmd_import, cmd_list, cmd_terminate, NULL
};

enum cmd_result cmd_capabilities(struct strbuf* line)
{
	if(strcmp(line->buf, "capabilities"))
		return NOT_HANDLED;

	printf("import\n");
	printf("refspec %s:%s\n", remote_ref, private_ref);
	printf("\n");
	fflush(stdout);
	return SUCCESS;
}

/* NOTE: 'ref' refers to a git reference, while 'rev' refers to a svn revision. */
static char *read_ref_note(const unsigned char sha1[20]) {
	/* read highest imported ref from the note attached to the latest commit */
	const unsigned char *note_sha1;
	char *msg = NULL;
	unsigned long msglen;
	enum object_type type;
	init_notes(NULL, notes_ref, NULL, 0);
	if(	(note_sha1 = get_note(NULL, sha1)) == NULL ||
			!(msg = read_sha1_file(note_sha1, &type, &msglen)) ||
			!msglen || type != OBJ_BLOB) {
		free(msg);
		return NULL;
	}
	return msg;
}

static int parse_rev_note(const char *msg, struct rev_note *res) {
	const char *key, *value, *end;
	size_t len;
	while(*msg) {
		end = strchr(msg, '\n');
		len = end ? end - msg : strlen(msg);

		key = "Revision-number: ";
		if(!prefixcmp(msg, key)) {
			long i;
			value = msg + strlen(key);
			i = atol(value);
			if(i < 0 || i > UINT32_MAX)
				return 1;
			res->rev_nr = i;
		}
		msg += len + 1;
	}
	return 0;
}
enum cmd_result cmd_import(struct strbuf* line)
{
	static int batch_active;
	int code, report_fd;
	char* back_pipe_env;
	int dumpin_fd;
	char *note_msg;
	unsigned char head_sha1[20];
	unsigned int startrev;
	struct argv_array svndump_argv = ARGV_ARRAY_INIT;
	struct child_process svndump_proc = {
			.argv = NULL,		/* comes later .. */
			/* we want a pipe to the child's stdout, but stdin, stderr inherited.
			 The user can be asked for e.g. a password */
			.in = 0, .out = -1, .err = 0,
			.no_stdin = 0, .no_stdout = 0, .no_stderr = 0,
			.git_cmd = 0,
			.silent_exec_failure = 0,
			.stdout_to_stderr = 0,
			.use_shell = 0,
			.clean_on_exit = 0,
			.preexec_cb = NULL,
			.env = NULL,
			.dir = NULL
	};

	/* terminate a current batch's fast-import stream */
	if(line->len == 0 && batch_active) {
		printf("done\n");
		fflush(stdout);
		batch_active = 0;
		printd("import-batch finished.");
		/*
		 * should the remote helper terminate after a batch?
		 * It seems that it should.
		 */
		return TERMINATE;
	}
	if(prefixcmp(line->buf, "import"))
		return NOT_HANDLED;

	/* import commands can be grouped together in a batch. Batches are ended by \n */
	batch_active = 1;

	back_pipe_env = getenv("GIT_REPORT_FIFO");
	if(!back_pipe_env) {
		die("Cannot get cat-blob-pipe from environment!");
	}

	/* opening a fifo for usually reading blocks until a writer has opened it too.
	 * Therefore, we open with RDWR.
	 */
	report_fd = open(back_pipe_env, O_RDWR);
	if(report_fd < 0) {
		die("Unable to open fast-import back-pipe! %s", strerror(errno));
	}

	printd("Opened fast-import back-pipe %s for reading.", back_pipe_env);

	if(read_ref(private_ref, head_sha1)) {
		printd("New branch");
		startrev = 0;
	} else {
		note_msg = read_ref_note(head_sha1);
		if(note_msg == NULL) {
			warning("No note found for %s.", private_ref);
			startrev = 0;
		}
		else {
			struct rev_note note = { 0 };
			printd("Note found for %s", private_ref);
			printd("Note:\n%s", note_msg);
			parse_rev_note(note_msg, &note);
			startrev = note.rev_nr + 1;
			free(note_msg);
		}
	}

	printd("starting import from revision %u", startrev);

	if(dump_from_file) {
		dumpin_fd = open(url, O_RDONLY);
		if(dumpin_fd < 0) {
			die_errno("Couldn't open svn dump file %s.", url);
		}
	}
	else {

		argv_array_push(&svndump_argv, "svnrdump");
		argv_array_push(&svndump_argv, "dump");
		argv_array_push(&svndump_argv, url);
		argv_array_pushf(&svndump_argv, "-r%u:HEAD", startrev);
		svndump_proc.argv = svndump_argv.argv;;

		code = start_command(&svndump_proc);
		if(code)
			die("Unable to start %s, code %d", svndump_proc.argv[0], code);
		dumpin_fd = svndump_proc.out;
	}

	svndump_init_fd(dumpin_fd, report_fd);
	svndump_read(url, private_ref, notes_ref);
	svndump_deinit();
	svndump_reset();

	close(dumpin_fd);
	close(report_fd);
	if(!dump_from_file)
		code = finish_command(&svndump_proc);
	if(code)
		warning("%s, returned %d", svndump_proc.argv[0], code);
	argv_array_clear(&svndump_argv);

	return SUCCESS;
}

enum cmd_result cmd_list(struct strbuf* line)
{
	if(strcmp(line->buf, "list"))
		return NOT_HANDLED;

	printf("? HEAD\n");
	printf("? %s\n\n", remote_ref);
	fflush(stdout);
	return SUCCESS;
}

enum cmd_result cmd_terminate(struct strbuf *line)
{
	/* an empty line terminates the program, if not in a batch sequence */
	if (line->len == 0)
		return TERMINATE;
	else
		return NOT_HANDLED;
}
enum cmd_result do_command(struct strbuf* line)
{
	const command* p = command_list;
	enum cmd_result ret;
	printd("command line '%s'", line->buf);
	while(*p) {
		ret = (*p)(line);
		if(ret != NOT_HANDLED)
			return ret;
		p++;
	}
	warning("Unknown command '%s'\n", line->buf);
	return ret;
}

int main(int argc, const char **argv)
{
	struct strbuf buf = STRBUF_INIT;
	int nongit;
	static struct remote* remote;

	if (getenv("GIT_TRANSPORT_HELPER_DEBUG"))
		debug = 1;

	git_extract_argv0_path(argv[0]);
	setup_git_directory_gently(&nongit);
	if (argc < 2) {
		fprintf(stderr, "Remote needed\n");
		return 1;
	}

	remote = remote_get(argv[1]);
	if (argc == 3) {
		url = argv[2];
	} else if (argc == 2) {
		url = remote->url[0];
	} else {
		warning("Excess arguments!");
	}

	if (!prefixcmp(url, "file://")) {
		dump_from_file = 1;
		url = url_decode(url + sizeof("file://")-1);
		printd("remote-svn uses a file as dump input.");
	}
	else {
		dump_from_file = 0;
		end_url_with_slash(&buf, url);
		url = strbuf_detach(&buf, NULL);
	}

	printd("remote-svn starting with url %s", url);

	strbuf_init(&buf, 0);
	strbuf_addf(&buf, "refs/svn/%s/master", remote->name);
	private_ref = strbuf_detach(&buf, NULL);
	strbuf_init(&buf, 0);
	strbuf_addf(&buf, "refs/notes/%s/revs", remote->name);
	notes_ref = strbuf_detach(&buf, NULL);

	while(1) {
		if (strbuf_getline(&buf, stdin, '\n') == EOF) {
			if (ferror(stdin))
				fprintf(stderr, "Error reading command stream\n");
			else
				fprintf(stderr, "Unexpected end of command stream\n");
			return 1;
		}
		if(do_command(&buf) == TERMINATE)
			break;
		strbuf_reset(&buf);
	}

	strbuf_release(&buf);
	free((void*)url);
	free((void*)private_ref);
	free((void*)notes_ref);
	return 0;
}
