#include "git-compat-util.h"
#include "line_buffer.h"
#include "svndiff.h"

#define DEBUG 1

#define SVN_DELTA_WINDOW_SIZE 102400
#define MAX_ENCODED_INT_LEN 10
#define MAX_INSTRUCTION_LEN (2*MAX_ENCODED_INT_LEN+1)
#define MAX_INSTRUCTION_SECTION_LEN (SVN_DELTA_WINDOW_SIZE*MAX_INSTRUCTION_LEN)
static char buf[SVN_DELTA_WINDOW_SIZE];

/* Remove when linking to gitcore */
void die(const char *err, ...)
{
	va_list params;
	va_start(params, err);
	vfprintf(stderr, err, params);
	printf("\n");
	va_end(params);
	exit(128);
}

size_t read_one_size()
{
	unsigned char c;
	size_t result = 0;

	while (1)
	{
		fread(&c, 1, 1, stdin);
		result = (result << 7) | (c & 127);
		if (!(c & 128))
			/* No continuation bit */
			break;
	}
	if (DEBUG)
		fprintf(stderr, "Reading size: %d\n", result);
	return result;
}

void read_one_instruction(struct svndiff_instruction *op)
{
	size_t c, action;

	/* Read the 1-byte instruction-selector */
	fread(&c, 1, 1, stdin);

	/* Decode the instruction selector from the two higher order
	   bits; the remaining 6 bits may contain the length */
	action = (c >> 6) & 3;
	if (action >= 3)
		die("Invalid instruction %d", action);

	op->action_code = (enum svndiff_action)(action);

	/* Attempt to extract the length length from the remaining
	   bits */
	op->length = c & 63;
	if (op->length == 0)
	{
		op->length = read_one_size();
		if (c == 0)
			die("Zero length instruction");
	}
	/* Offset is present if action is copyfrom_source or
	   copyfrom_target */
	if (action != copyfrom_new)
		op->offset = read_one_size();

}

size_t read_instructions(struct svndiff_window *window)
{
	size_t tpos = 0, npos = 0, ninst = 0;
	struct svndiff_instruction *op;

	while (tpos <= window->ins_len)
	{
		window->ops = realloc(window->ops, ++ninst);
		op = window->ops + ninst - 1;
		read_one_instruction(op);

		if (DEBUG)
			fprintf(stderr, "Instruction: %d %d %d\n",
				op->action_code, op->offset, op->length);

		if (op == NULL)
			die("Invalid diff stream: "
			    "instruction %d cannot be decoded", ninst);
		else if (op->length == 0)
			die("Invalid diff stream: "
			    "instruction %d has length zero", ninst);
		else if (op->length > window->tview_len - tpos)
			die("Invalid diff stream: "
			    "instruction %d overflows the target view", ninst);

		switch (op->action_code)
		{
		case copyfrom_source:
			if (op->length > window->sview_len - op->offset ||
			    op->offset > window->sview_len)
				die("Invalid diff stream: "
				    "[src] instruction %d overflows "
				    " the source view", ninst);
			break;
		case copyfrom_target:
			if (op->offset >= tpos)
				die("Invalid diff stream: "
				    "[tgt] instruction %d starts "
				    "beyond the target view position", ninst);
			break;
		case copyfrom_new:
			if (op->length > window->newdata_len - npos)
				die("Invalid diff stream: "
				    "[new] instruction %d overflows "
				    "the new data section", ninst);
			npos += op->length;
			break;
		}
		tpos += op->length;
	}

	if (tpos != window->tview_len)
		die("Delta does not fill the target window");
	if (npos != window->newdata_len)
		die("Delta does not contain enough new data");
	return ninst;
}

void read_window_header(struct svndiff_window *window)
{
	/* Read five sizes; various offsets and lengths */
	window->sview_offset = read_one_size();
	window->sview_len = read_one_size();
	window->tview_len = read_one_size();
	window->ins_len = read_one_size();
	window->newdata_len = read_one_size();

	if (window->tview_len > SVN_DELTA_WINDOW_SIZE ||
	    window->sview_len > SVN_DELTA_WINDOW_SIZE ||
	    window->newdata_len > SVN_DELTA_WINDOW_SIZE + MAX_ENCODED_INT_LEN ||
	    window->ins_len > MAX_INSTRUCTION_SECTION_LEN)
		die("Svndiff contains a window that's too large");

	/* Check for integer overflow */
	if (window->ins_len + window->newdata_len < window->ins_len
	    || window->sview_len + window->tview_len < window->sview_len
	    || window->sview_offset + window->sview_len < window->sview_offset)
		die("Svndiff contains corrupt window header");

	if (DEBUG)
		fprintf(stderr, "Window header: %d %d %d %d %d\n",
			window->sview_offset, window->sview_len,
			window->tview_len, window->ins_len, window->newdata_len);
}

void drive_window(struct svndiff_window *window, FILE *src_fd)
{
	struct svndiff_instruction *op;
	size_t ninst;
	FILE *target_fd;
	long target_fd_end;

	/* Populate the first five fields of the the window object
	   with data from the stream */	
	read_window_header(window);

	/* Read instructions of length ins_len into window->ops
	   performing memory allocations as necessary */
	ninst = read_instructions(window);

	/* The Applier */
	/* We're now looking at new_data; read ahead only in the
	   copyfrom_new case */	
	target_fd = tmpfile();
	for (op = window->ops; ninst-- > 0; op++) {
		if (DEBUG) {
			fprintf(stderr, "op: %u %u %u\n", op->action_code,
				op->offset, op->length);
		}
		switch (op->action_code) {
		case copyfrom_source:
			fseek(src_fd, op->offset, SEEK_SET);
			fread(buf, op->length, 1, src_fd);
			fwrite(buf, op->length, 1, target_fd);
			break;
		case copyfrom_target:
			fseek(target_fd, op->offset, SEEK_SET);
			fread(buf, op->length, 1, target_fd);
			fseek(target_fd, 0, SEEK_END);
			fwrite(buf, op->length, 1, target_fd);
			break;
		case copyfrom_new:
			fseek(target_fd, 0, SEEK_END);
			buffer_copy_bytes(op->length, target_fd);
			break;
		}
	}
	target_fd_end = ftell(target_fd);
	fseek(target_fd, 0, SEEK_SET);
	fread(buf, target_fd_end, 1, target_fd);
	fwrite(buf, target_fd_end, 1, stdout);
	fclose (target_fd);
}

int main(int argc, char **argv)
{
	int version;
	struct svndiff_window *window;
	FILE *src_fd;
	buffer_init(NULL);

	/* Read off the 4-byte header: "SVN\0" */
	fread(&buf, 4, 1, stdin);
	version = atoi(buf + 3);
	if (version != 0)
		die("Version %d unsupported", version);

	/* Setup the source to apply windows to */
	src_fd = fopen(argv[1], "r");
	
	/*  Now start processing the windows: feof(stdin) is the
	    external context to indicate that there's no more svndiff
	    data, since there's no end marker in the svndiff0
	    format */
	while (!feof(stdin)) {
		window = malloc(sizeof(*window));
		drive_window(window, src_fd);
		free(window);
	}
	buffer_deinit();
	return 0;
}
