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

/* Return the number of bytes read */
size_t read_one_size(size_t *size)
{
	unsigned char c;
	size_t result, bsize;
	result = 0;
	bsize = 0;

	while (1)
	{
		fread(&c, 1, 1, stdin);
		result = (result << 7) | (c & 127);
		bsize ++;
		if (!(c & 128))
			/* No continuation bit */
			break;
	}
	*size = result;
	return bsize;
}

/* Return the number of bytes read */
size_t read_one_instruction(struct svndiff_instruction *op)
{
	unsigned char c;
	size_t action, bsize;
	bsize = 0;

	/* Read the 1-byte instruction-selector */
	fread(&c, 1, 1, stdin);
	bsize ++;

	/* Decode the instruction selector from the two higher order
	   bits; the remaining 6 bits may contain the length */
	action = (c >> 6) & 3;
	if (action >= 3)
		die("Invalid instruction %"PRIu64, (uint64_t) action);

	op->action_code = (enum svndiff_action)(action);

	/* Attempt to extract the length length from the remaining
	   bits */
	op->length = c & 63;
	if (op->length == 0)
	{
		bsize += read_one_size(&(op->length));
		if (op->length == 0)
			die("Zero length instruction");
	}
	/* Offset is present if action is copyfrom_source or
	   copyfrom_target */
	if (action != copyfrom_new)
		bsize += read_one_size(&(op->offset));
	return bsize;
}

size_t read_instructions(struct svndiff_window *window, size_t *ninst)
{
	size_t tpos = 0, npos, bsize;
	struct svndiff_instruction *op;
	npos = 0;
	bsize = 0;
	*ninst = 0;
	
	while (bsize < window->ins_len)
	{
		++(*ninst);
		window->ops = realloc(window->ops, (*ninst) * sizeof(*op));
		op = window->ops + (*ninst) - 1;
		bsize += read_one_instruction(op);

		if (DEBUG)
			fprintf(stderr,
				"Instruction: %"PRIu64" %"PRIu64" %"PRIu64" (%"PRIu64")\n",
				(uint64_t) op->action_code,
				(uint64_t) op->length,
				(uint64_t) op->offset,
				(uint64_t) bsize);

		if (op == NULL)
			die("Invalid diff stream: "
				"instruction %"PRIu64" cannot be decoded", (uint64_t) *ninst);
		else if (op->length == 0)
			die("Invalid diff stream: "
				"instruction %"PRIu64" has length zero", (uint64_t) *ninst);
		else if (op->length > window->tview_len - tpos)
			die("Invalid diff stream: "
				"instruction %"PRIu64" overflows the target view",
			(uint64_t) *ninst);

		switch (op->action_code)
		{
		case copyfrom_source:
			if (op->length > window->sview_len - op->offset ||
				op->offset > window->sview_len)
				die("Invalid diff stream: "
					"[src] instruction %"PRIu64" overflows "
					" the source view", (uint64_t) *ninst);
			break;
		case copyfrom_target:
			if (op->offset >= tpos)
				die("Invalid diff stream: "
					"[tgt] instruction %"PRIu64" starts "
					"beyond the target view position", (uint64_t) *ninst);
			break;
		case copyfrom_new:
			if (op->length > window->newdata_len - npos)
				die("Invalid diff stream: "
					"[new] instruction %"PRIu64" overflows "
					"the new data section", (uint64_t) *ninst);
			npos += op->length;
			break;
		}
		tpos += op->length;
	}

	if (tpos != window->tview_len)
		die("Delta does not fill the target window");
	if (npos != window->newdata_len)
		die("Delta does not contain enough new data");
	return bsize;
}

size_t read_window_header(struct svndiff_window *window)
{
	size_t bsize = 0;

	/* Read five sizes; various offsets and lengths */
	bsize += read_one_size(&(window->sview_offset));
	bsize += read_one_size(&(window->sview_len));
	bsize += read_one_size(&(window->tview_len));
	bsize += read_one_size(&(window->ins_len));
	bsize += read_one_size(&(window->newdata_len));

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
		fprintf(stderr,
			"Window header: %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64"\n",
			(uint64_t) window->sview_offset,
			(uint64_t) window->sview_len,
			(uint64_t) window->tview_len,
			(uint64_t) window->ins_len,
			(uint64_t) window->newdata_len);
	return bsize;
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
	read_instructions(window, &ninst);

	/* The Applier */
	/* We're now looking at new_data; read ahead only in the
	   copyfrom_new case */	
	target_fd = tmpfile();
	for (op = window->ops; ninst-- > 0; op++) {
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
	free(window->ops);
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

	/* Read and drive the first window */
	window = malloc(sizeof(*window));
	drive_window(window, src_fd);
	free(window);

	buffer_deinit();
	return 0;
}
