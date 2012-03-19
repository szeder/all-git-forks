
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>

#include <assert.h>

#include <regex.h>

extern int errno;

#define die(fmt, arg...)						\
	do {                                                            \
		fprintf(stderr, "line %d: fatal error, " fmt "\n",	\
			__LINE__, ##arg);				\
		fprintf(stderr, "errno: %s\n", strerror(errno));	\
		exit(1);						\
	} while (0)

#define ETX 0x03

void *xalloc(size_t size)
{
	void *ret;

	ret = calloc(sizeof(char), size);
	if (!ret)
		die("memory allocation failed");

	return ret;
}

void *xrealloc(void *ptr, size_t size)
{
	void *ret;

	assert(size);
	ret = realloc(ptr, size);
	if (!ret)
		die("memory allocation failed");

	return ret;
}

int stdin_fd = 0, tty_fd;
unsigned int row, col;
int running;

#define LINES_INIT_SIZE 128

void update_row_col(void)
{
	struct winsize size;

	bzero(&size, sizeof(struct winsize));
	ioctl(tty_fd, TIOCGWINSZ, (void *)&size);

	row = size.ws_row;
	col = size.ws_col;
}

void signal_handler(int signum)
{
	void update_terminal(void); /* FIXME */

	switch (signum) {
	case SIGWINCH:
		update_row_col();
		update_terminal();
		break;

	case SIGINT:
		running = 0;
		break;

	default:
		die("unknown signal: %d", signum);
		break;
	}
}

struct termios attr;

void init_tty(void)
{
	struct sigaction act;

	tty_fd = open("/dev/tty", O_RDONLY);
	if (tty_fd < 0)
		die("open()ing /dev/tty");

	bzero(&attr, sizeof(struct termios));
	tcgetattr(tty_fd, &attr);
	attr.c_lflag &= ~ICANON;
	attr.c_lflag &= ~ECHO;
	tcsetattr(tty_fd, TCSANOW, &attr);

	bzero(&act, sizeof(struct sigaction));
	act.sa_handler = signal_handler;
	sigaction(SIGWINCH, &act, NULL);
	sigaction(SIGINT, &act, NULL);

	update_row_col();
}

char *logbuf;
int logbuf_size, logbuf_used;
#define LOGBUF_INIT_SIZE 1024

int last_etx;

struct commit {
	unsigned int *lines;	/* array of index of logbuf */
	int lines_size, lines_used;

	int nr_lines, head_line;

	/*
	 * caution:
	 * prev means previous commit of the commit object,
	 * next means next commit of the commit object.
	 */
	struct commit *prev, *next;
};

/* head: HEAD, root: root of the commit tree */
struct commit *head, *root;
/* current: current displaying commit, tail: tail of the read commits */
struct commit *current, *tail;

void update_terminal(void)
{
	int i, j;
	char *line;

	printf("\033[2J\033[0;0J");

	for (i = current->head_line; i < current->head_line + row
		     && i < current->nr_lines; i++) {
		line = &logbuf[current->lines[i]];

		/* FIXME: first new line should be eliminated in git-log */
		if (current != head && i == 0)
			continue;

		for (j = 0; j < col && line[j] != '\n'; j++)
			putchar(line[j]);
		putchar('\n');
	}
}

void init_commit(struct commit *c, int first_index)
{
	int i, line_head;

	c->lines_size = LINES_INIT_SIZE;
	c->lines = xalloc(c->lines_size * sizeof(int));

	line_head = first_index;

	for (i = first_index; logbuf[i] != ETX; i++) {
		if (logbuf[i] != '\n')
			continue;

		c->nr_lines++;
		c->lines[c->lines_used++] = line_head;
		line_head = i + 1;

		if (c->lines_size == c->lines_used) {
			c->lines_size <<= 1;
			c->lines = xrealloc(c->lines, c->lines_size * sizeof(int));
		}
	}

	c->nr_lines++;
	c->lines[c->lines_used++] = line_head;
}

int contain_etx(int begin, int end)
{
	int i;

	for (i = begin; i < end; i++)
		if (logbuf[i] == (char)ETX) return i;

	return -1;
}

void read_head(void)
{
	int prev_logbuf_used;

	do {
		int rbyte;

		if (logbuf_used == logbuf_size) {
			logbuf_size <<= 1;
			logbuf = xrealloc(logbuf, logbuf_size);
		}

		rbyte = read(stdin_fd, &logbuf[logbuf_used],
			logbuf_size - logbuf_used);

		if (rbyte < 0) {
			if (errno == EINTR)
				continue;
			else
				die("read() failed");
		}

		if (!rbyte)
			exit(0); /* no input */

		prev_logbuf_used = logbuf_used;
		logbuf_used += rbyte;
	} while ((last_etx = contain_etx(prev_logbuf_used, logbuf_used)) == -1);

	head = xalloc(sizeof(struct commit));
	init_commit(head, 0);

	tail = current = head;
}

void read_at_least_one_commit(void)
{
	int prev_last_etx = last_etx;
	int prev_logbuf_used;
	struct commit *new_commit;

	if (last_etx + 1 < logbuf_used) {
		unsigned int tmp_etx;

		tmp_etx = contain_etx(last_etx + 1, logbuf_used);
		if (tmp_etx != -1) {
			last_etx = tmp_etx;
			goto skip_read;
		}
	}

	do {
		int rbyte;

		if (logbuf_used == logbuf_size) {
			logbuf_size <<= 1;
			logbuf = xrealloc(logbuf, logbuf_size);
		}

		rbyte = read(stdin_fd, &logbuf[logbuf_used], logbuf_size - logbuf_used);

		if (rbyte < 0) {
			if (errno == EINTR)
				continue;
			else
				die("read() failed");
		}

		if (!rbyte)
			return;

		prev_logbuf_used = logbuf_used;
		logbuf_used += rbyte;
	} while ((last_etx = contain_etx(prev_logbuf_used, logbuf_used)) == -1);

skip_read:

	new_commit = xalloc(sizeof(struct commit));

	assert(!tail->prev);
	tail->prev = new_commit;
	new_commit->next = tail;

	tail = new_commit;

	init_commit(new_commit, prev_last_etx + 1);
}

int show_prev_commit(void)
{
	if (!current->prev) {
		read_at_least_one_commit();

		if (!current->prev)
			return 0;
	}

	current = current->prev;
	return 1;
}

int show_next_commit(void)
{
	if (!current->next) {
		assert(current == head);
		return 0;
	}

	current = current->next;
	return 1;
}

int forward_line(void)
{
	if (current->head_line + row < current->nr_lines) {
		current->head_line++;
		return 1;
	}

	return 0;
}

int backward_line(void)
{
	if (0 < current->head_line) {
		current->head_line--;
		return 1;
	}

	return 0;
}

int goto_top(void)
{
	if (!current->head_line)
		return 0;

	current->head_line = 0;
	return 1;
}

int goto_bottom(void)
{
	if (current->nr_lines < row)
		return 0;

	current->head_line = current->nr_lines - row;
	return 1;
}

int forward_page(void)
{
	if (current->nr_lines < current->head_line + row)
		return 0;

	current->head_line += row;
	return 1;
}

int backward_page(void)
{
	if (!current->head_line)
		return 0;

	if (0 < current->head_line - row) {
		current->head_line = 0;
		return 1;
	}

	current->head_line -= row;
	return 1;
}

int show_root(void)
{
	if (root) {
		current = root;
		return 1;
	}

	do {
		if (!current->prev)
			read_at_least_one_commit();
		if (!current->prev)
			break;
		current = current->prev;
	} while (1);

	assert(!root);
	root = current;

	return 1;
}

int show_head(void)
{
	if (current == head)
		return 0;

	current = head;
	return 1;
}

int nop(void)
{
	return 0;
}

int quit(void)
{
	exit(0);		/* never return */
	return 0;
}

struct key_cmd {
	char key;
	int (*op)(void);
};

struct key_cmd valid_ops[] = {
	{ 'h', show_prev_commit },
	{ 'j', forward_line },
	{ 'k', backward_line },
	{ 'l', show_next_commit },
	{ 'q', quit },
	{ 'g', goto_top },
	{ 'G', goto_bottom },
	{ ' ', forward_page },
	{ 'J', forward_page },
	{ 'K', backward_page },
	{ 'H', show_root },
	{ 'L', show_head },

	/* todo: '/' forward search, '?' backword search */
	{ '\0', NULL },
};

int (*ops_array[256])(void);

int main(void)
{
	int i;
	char cmd;

	init_tty();

	logbuf = xalloc(LOGBUF_INIT_SIZE);
	logbuf_size = LOGBUF_INIT_SIZE;
	logbuf_used = 0;
	read_head();

	update_terminal();

	for (i = 0; i < 256; i++)
		ops_array[i] = nop;

	for (i = 0; valid_ops[i].key != '\0'; i++)
		ops_array[valid_ops[i].key] = valid_ops[i].op;

	running = 1;
	while (running) {
		int read_ret;

		read_ret = read(tty_fd, &cmd, 1);

		if (read_ret == -1 && errno == EINTR) {
			if (!running)
				break;

			errno = 0;
			continue;
		}

		if (read_ret != 1)
			die("reading key input failed");

		if (ops_array[cmd]())
			update_terminal();
	}

	return 0;
}
