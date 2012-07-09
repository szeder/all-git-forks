
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

static struct remote* remote;
static const char* url;
static int dump_from_file;
const char* private_refs = "refs/remote-svn/";		/* + remote->name. */
const char* remote_ref = "refs/heads/master";

enum cmd_result cmd_capabilities(struct strbuf* line);
enum cmd_result cmd_import(struct strbuf* line);
enum cmd_result cmd_list(struct strbuf* line);

enum cmd_result { SUCCESS, NOT_HANDLED, ERROR };
typedef enum cmd_result (*command)(struct strbuf*);

const command command_list[] = {
		cmd_capabilities, cmd_import, cmd_list, NULL
};

enum cmd_result cmd_capabilities(struct strbuf* line)
{
	if(strcmp(line->buf, "capabilities"))
		return NOT_HANDLED;

	printf("import\n");
	printf("\n");
	fflush(stdout);
	return SUCCESS;
}

enum cmd_result cmd_import(struct strbuf* line)
{
	const char* revs = "-r0:HEAD";
	int code, report_fd;
	char* back_pipe_env;
	int dumpin_fd;
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

	if(prefixcmp(line->buf, "import"))
		return NOT_HANDLED;

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

	if(dump_from_file) {
		dumpin_fd = open(url, O_RDONLY);
		if(dumpin_fd < 0) {
			die_errno("Couldn't open svn dump file %s.", url);
		}
	}
	else {
		svndump_proc.argv = xcalloc(5, sizeof(char*));
		svndump_proc.argv[0] = "svnrdump";
		svndump_proc.argv[1] = "dump";
		svndump_proc.argv[2] = url;
		svndump_proc.argv[3] = revs;

		code = start_command(&svndump_proc);
		if(code)
			die("Unable to start %s, code %d", svndump_proc.argv[0], code);
		dumpin_fd = svndump_proc.out;
	}


	svndump_init_fd(dumpin_fd, report_fd);
	svndump_read(url);
	svndump_deinit();
	svndump_reset();

	close(dumpin_fd);
	close(report_fd);
	if(!dump_from_file)
		code = finish_command(&svndump_proc);
	if(code)
		warning("Something went wrong with termination of %s, code %d", svndump_proc.argv[0], code);
	free(svndump_proc.argv);

	printf("done\n");
	return SUCCESS;



}

enum cmd_result cmd_list(struct strbuf* line)
{
	if(strcmp(line->buf, "list"))
		return NOT_HANDLED;

	printf("? HEAD\n");
	printf("? %s\n", remote_ref);
	printf("\n");
	fflush(stdout);
	return SUCCESS;
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

	/* build private ref namespace path for this svn remote. */
	strbuf_init(&buf, 0);
	strbuf_addstr(&buf, private_refs);
	strbuf_addstr(&buf, remote->name);
	strbuf_addch(&buf, '/');
	private_refs = strbuf_detach(&buf, NULL);

	while(1) {
		if (strbuf_getline(&buf, stdin, '\n') == EOF) {
			if (ferror(stdin))
				fprintf(stderr, "Error reading command stream\n");
			else
				fprintf(stderr, "Unexpected end of command stream\n");
			return 1;
		}
		/* an empty line terminates the command stream */
		if(buf.len == 0)
			break;

		do_command(&buf);
		strbuf_reset(&buf);
	}

	strbuf_release(&buf);
	free((void*)url);
	free((void*)private_refs);
	return 0;
}
