/*
 * gitless - a specialized pager for git-log
 *
 * Copyright (C) 2012 Hitoshi Mitake <h.mitake@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

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

int ret_nl_index(char *s)
{
	int i;

	for (i = 0; s[i] != '\n'; i++);
	return i;
}

int stdin_fd = 0, tty_fd;
unsigned int row, col;
int running = 1;

#define LINES_INIT_SIZE 128

enum {
	STATE_DEFAULT,
	STATE_INPUT_SEARCH_QUERY,
	STATE_SEARCHING_QUERY
};
int state = STATE_DEFAULT;

#define BOTTOM_MESSAGE_INIT_SIZE 32
char *bottom_message;
int bottom_message_size = BOTTOM_MESSAGE_INIT_SIZE;
int print_bottom_message;

#define MATCH_ARRAY_INIT_SIZE 32
regmatch_t *match_array;
int match_array_size = MATCH_ARRAY_INIT_SIZE;

#define bmprintf(fmt, arg...)						\
	do {                                                            \
		snprintf(bottom_message, bottom_message_size,		\
			fmt, ##arg);					\
	} while (0)

void update_row_col(void)
{
	struct winsize size;

	bzero(&size, sizeof(struct winsize));
	ioctl(tty_fd, TIOCGWINSZ, (void *)&size);

	row = size.ws_row - 1;
	col = size.ws_col;

	if (bottom_message_size - 1 < col) {
		bottom_message_size = col + 1;
		bottom_message = xrealloc(bottom_message, bottom_message_size);
	}

	if (match_array_size < col) {
		match_array_size = col;
		match_array = xrealloc(match_array, match_array_size * sizeof(regmatch_t));
	}
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

regex_t re_compiled;

void update_terminal(void)
{
	int i, j, print;
	char *line;

	printf("\033[2J\033[0;0J");

	/* FIXME: first new line should be eliminated in git-log */
	if (current != head && !current->head_line)
		current->head_line = 1;

	for (i = current->head_line, print = 0;
	     i < current->head_line + row
		     && i < current->nr_lines; i++, print++) {
		line = &logbuf[current->lines[i]];

		if (state == STATE_SEARCHING_QUERY) {
			int ret, mi, nli = ret_nl_index(line);
			int rev = 0;

			line[nli] = '\0';
			ret = regexec(&re_compiled, line,
				match_array_size, match_array, 0);
			line[nli] = '\n';

			if (ret)
				goto normal_print;

			for (mi = j = 0; j < col && line[j] != '\n'; j++) {
				if (j == match_array[mi].rm_so) {
					printf("\033[7m");
					rev = 1;
				} else if (j == match_array[mi].rm_eo) {
					printf("\033[0m");
					rev = 0;

					mi++;
				}

				if (match_array[mi].rm_so
					== match_array[mi].rm_eo) {
					printf("\033[0m");
					rev = 0;

					mi++;
				}

				putchar(line[j]);
			}

			if (rev)
				printf("\033[0m");
		} else {
		normal_print:
			for (j = 0; j < col && line[j] != '\n'; j++)
				putchar(line[j]);
		}
		putchar('\n');
	}

	while (i++ < current->head_line + row)
		putchar('\n');

	if (current->nr_lines <= current->head_line + row)
		printf("\033[7m100%%\033[0m");
	else
		printf("\033[7m% .0f%%\033[0m",
			(float)(current->head_line + row)
			/ current->nr_lines * 100.0);

	printf("\033[7m %s\033[0m", bottom_message);
	fflush(stdout);
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

int show_prev_commit(char cmd)
{
	if (!current->prev) {
		read_at_least_one_commit();

		if (!current->prev)
			return 0;
	}

	current = current->prev;
	return 1;
}

int show_next_commit(char cmd)
{
	if (!current->next) {
		assert(current == head);
		return 0;
	}

	current = current->next;
	return 1;
}

int forward_line(char cmd)
{
	if (current->head_line + row < current->nr_lines) {
		current->head_line++;
		return 1;
	}

	return 0;
}

int backward_line(char cmd)
{
	if (0 < current->head_line) {
		current->head_line--;
		return 1;
	}

	return 0;
}

int goto_top(char cmd)
{
	if (!current->head_line)
		return 0;

	current->head_line = 0;
	return 1;
}

int goto_bottom(char cmd)
{
	if (current->nr_lines < row)
		return 0;

	current->head_line = current->nr_lines - row;
	return 1;
}

int forward_page(char cmd)
{
	if (current->nr_lines < current->head_line + row)
		return 0;

	current->head_line += row;
	return 1;
}

int backward_page(char cmd)
{
	if (!current->head_line)
		return 0;

	if (current->head_line - row <= 0) {
		current->head_line = 0;
		return 1;
	}

	current->head_line -= row;
	return 1;
}

int show_root(char cmd)
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

int show_head(char cmd)
{
	if (current == head)
		return 0;

	current = head;
	return 1;
}

char read_one_key(void)
{
	int rbyte;
	char buf;

try_again:
	rbyte = read(tty_fd, &buf, 1);
	if (rbyte < 0) {
		if (errno == EINTR)
			goto try_again;
		die("read() failed");
	}
	assert(rbyte == 1);

	return buf;
}

#define QUERY_SIZE 128
char query[QUERY_SIZE + 1];
int query_used;

int match_line(char *line)
{
	if (!regexec(&re_compiled, line, 0, NULL, REG_NOTEOL))
		return 1;

	return 0;
}

int match_commit(struct commit *c, int direction, int prog)
{
	int i = c->head_line;
	int nli, result;
	char *line;

	if (prog) {
		if (direction) {
			if (i < c->nr_lines - 1)
				return 0;
		} else {
			if (!i)
				return 0;
		}

		i += direction ? 1 : -1;
	}

	do {
		line = &logbuf[c->lines[i]];
		nli = ret_nl_index(line);

		line[nli] = '\0';
		result = match_line(line);
		line[nli] = '\n';

		if (result) {
			c->head_line = i;
			return 1;
		}

		i += direction ? 1 : -1;
	} while (direction ? i < c->nr_lines : 0 <= i);

	return 0;
}

void do_search(int direction, int global, int prog)
{
	int result;
	struct commit *p;

	result = match_commit(current, direction, prog);
	if (result || !global)
		return;

	if (direction) {
		if (!current->prev)
			read_at_least_one_commit();

		if (current->prev)
			p = current->prev;
		else
			return;
	} else {
		if (current->next)
			p = current->next;
		else
			return;
	}

	do {
		if (match_commit(p, direction, prog))
			goto matched;

		if (!p->prev)
			read_at_least_one_commit();
	} while (direction ? (p = p->prev) : (p = p->next));

	return;

matched:
	current = p;
}

int current_direction, current_global;

#define update_query_bm()	do {					\
		bmprintf("\033[7m%s %s search:\033[0m %s",		\
			current_direction ? "forward" : "backward",	\
			current_global ? "global" : "local",		\
			query);						\
									\
		print_bottom_message = 1;				\
	} while (0)

int _search(int key, int direction, int global)
{
	current_direction = direction;
	current_global = global;

	switch (state) {
	case STATE_DEFAULT:
	case STATE_SEARCHING_QUERY:
		query_used = 0;
		bzero(query, QUERY_SIZE);

		update_query_bm();
		state = STATE_INPUT_SEARCH_QUERY;
		break;

	case STATE_INPUT_SEARCH_QUERY:
		if (query_used + 1 == QUERY_SIZE) {
			bmprintf("search query is too long!");
			state = STATE_DEFAULT;

			goto end;
		}

		if (key == '\n')
			state = STATE_SEARCHING_QUERY;
		else {
			query[query_used++] = (char)key;
			update_query_bm();
		}
	end:
		break;

	default:
		die("invalid or unknown state: %d", state);
		break;
	}

	if (state == STATE_SEARCHING_QUERY) {
		bzero(&re_compiled, sizeof(regex_t));
		/* todo: REG_ICASE should be optional */
		regcomp(&re_compiled, query, REG_ICASE);

		do_search(direction, global, 0);
	}

	return 1;
}

int search(int direction, int global)
{
	return _search(-1, direction, global);
}

int search_global_forward(char cmd)
{
	return search(1, 1);
}

int search_global_backward(char cmd)
{
	return search(0, 1);
}

int search_local_forward(char cmd)
{
	return search(1, 0);
}

int search_local_backward(char cmd)
{
	return search(0, 0);
}

int search_progress(char cmd)
{
	if (state != STATE_SEARCHING_QUERY)
		return 0;

	do_search(cmd == 'n' ? current_direction : !current_direction,
		current_global, 1);
}

int input_query(char key)
{
	if (key == (char)0x7f) {
		/* backspace */
		if (!query_used)
			return 0;

		query[--query_used] = '\0';
		update_query_bm();

		return 1;
	} else if (key == (char)0x1b) {
		/* escape */
		print_bottom_message = 0;
		regfree(&re_compiled);

		state = STATE_DEFAULT;
		return 1;
	}

	return _search(key, current_direction, current_global);
}

int nop(char cmd)
{
	return 0;
}

int quit(char cmd)
{
	running = 0;
	return 0;
}

struct key_cmd {
	char key;
	int (*op)(char);
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
	{ '/', search_global_forward },
	{ '?', search_global_backward },
	{ '\\', search_local_forward },
	{ '!', search_local_backward },
	{ 'n', search_progress },
	{ 'p', search_progress },

	/* todo: '/' forward search, '?' backword search */
	{ '\0', NULL },
};

int (*ops_array[256])(char);

int main(void)
{
	int i;
	char cmd;

	bottom_message = xalloc(bottom_message_size);
	match_array = xalloc(match_array_size * sizeof(regmatch_t));

	init_tty();

	logbuf_size = LOGBUF_INIT_SIZE;
	logbuf = xalloc(logbuf_size);
	logbuf_used = 0;
	read_head();

	update_terminal();

	for (i = 0; i < 256; i++)
		ops_array[i] = nop;

	for (i = 0; valid_ops[i].key != '\0'; i++)
		ops_array[valid_ops[i].key] = valid_ops[i].op;

	while (running) {
		int ret;

		ret = read(tty_fd, &cmd, 1);
		if (ret == -1 && errno == EINTR) {
			if (!running)
				break;

			errno = 0;
			continue;
		}

		if (ret != 1)
			die("reading key input failed");

		if (state == STATE_INPUT_SEARCH_QUERY)
			ret = input_query(cmd);
		else
			ret = ops_array[cmd](cmd);

		if (ret)
			update_terminal();
	}

	return 0;
}
