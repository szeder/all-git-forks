#include "git-compat-util.h"
#include "transport.h"


/*
 * URL syntax:
 *	'fd::<inoutfd>[/<anything>]'		Read/write socket pair
 *						<inoutfd>.
 *	'fd::<infd>,<outfd>[/<anything>]'	Read pipe <infd> and write
 *						pipe <outfd>.
 *	[foo] indicates 'foo' is optional. <anything> is any string.
 *
 * The data output to <outfd>/<inoutfd> should be passed unmolested to
 * git-receive-pack/git-upload-pack/git-upload-archive and output of
 * git-receive-pack/git-upload-pack/git-upload-archive should be passed
 * unmolested to <infd>/<inoutfd>.
 *
 */

static int input_fd = -1;
static int output_fd = -1;

#define MAXCOMMAND 4096

static int command_loop(void)
{
	char buffer[MAXCOMMAND];

	while (1) {
		if (!fgets(buffer, MAXCOMMAND - 1, stdin))
			exit(0);
		/* Strip end of line characters. */
		while (isspace((unsigned char)buffer[strlen(buffer) - 1]))
			buffer[strlen(buffer) - 1] = 0;

		if (!strcmp(buffer, "capabilities")) {
			printf("*connect\n\n");
			fflush(stdout);
		} else if (!strncmp(buffer, "connect ", 8)) {
			printf("\n");
			fflush(stdout);
			return bidirectional_transfer_loop(input_fd,
				output_fd);
		} else {
			fprintf(stderr, "Bad command");
			return 1;
		}
	}
}

int cmd_remote_fd(int argc, const char **argv, const char *prefix)
{
	char *end;
	unsigned long r;

	if (argc < 3) {
		fprintf(stderr, "Error: URL missing");
		exit(1);
	}

	r = strtoul(argv[2], &end, 10);
	input_fd = (int)r;

	if ((*end != ',' && *end != '/' && *end) || end == argv[2]) {
		fprintf(stderr, "Error: Bad URL syntax");
		exit(1);
	}

	if (*end == '/' || !*end) {
		output_fd = input_fd;
	} else {
		char *end2;
		r = strtoul(end + 1, &end2, 10);
		output_fd = (int)r;

		if ((*end2 != '/' && *end2) || end2 == end + 1) {
			fprintf(stderr, "Error: Bad URL syntax");
			exit(1);
		}
	}

	return command_loop();
}
