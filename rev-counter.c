#include "cache.h"
#include "strbuf.h"
#include "commit.h"
#include "commit-slab.h"

define_commit_slab(counter, char *);
struct counter counter = COMMIT_SLAB_INIT(1, counter);

static unsigned commit_counter;
static int run_counter;
int latest_fd = -1;
struct strbuf recent = STRBUF_INIT;

/* home row, except 'l' which looks too much like '1' */
const char *starter = "asdfghjk";

static void start_marking(void)
{
	char buf[64];
	ssize_t len;

	len = readlink(git_path("rev-counter/latest"), buf, sizeof(buf));
	if (len == -1) {
		if (errno != ENOENT)
			die_errno("failed to open rev-counter/latest");
	} else {
		char ch, *p;
		sscanf(buf, "%c", &ch);
		p = strchrnul(starter, ch);
		run_counter = ((p - starter) + 1) % strlen(starter);
	}

	if (!is_directory(git_path("rev-counter")))
		mkdir(git_path("rev-counter"), 0755);
	latest_fd = open(git_path("rev-counter/%c", starter[run_counter]),
			 O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (latest_fd == -1)
		die_errno("failed to create new rev-counter");

	sprintf(buf, "%c", starter[run_counter]);
	(void)unlink(git_path("rev-counter/latest"));
	if (symlink(buf, git_path("rev-counter/latest")))
		die_errno("failed to update rev-counter/latest");
}

static int strbuf_find_commit_mark(const struct strbuf *sb,
				   const char *mark,
				   struct object_id *oid)
{
	const char *p, *end;
	int ret = -1;

	end = sb->buf + sb->len;
	p = sb->buf;
	while (p < end && ret < 0) {
		const char *next = strchrnul(p, '\n');

		if (next - p > 41 &&
		    p[40] == ' ' &&
		    p + 41 + strlen(mark) == next &&
		    !strncmp(p + 41, mark, strlen(mark)) &&
		    !get_oid_hex(p, oid))
			ret = 0;

		p = next;
		if (*p == '\n')
			p++;
	}
	return ret;
}

void update_commit_counter(void)
{
	static int read_recent;
	struct object_id oid;
	char buf[16];

	if (!read_recent) {
		strbuf_read_file(&recent, git_path("rev-counter/recent"), 0);
		read_recent = 1;

		if (recent.len > 1000 * 45) { /* prune time? */
			int fd;
			const char *p = strchr(recent.buf + 1000 * 45, '\n');
			strbuf_remove(&recent, 0, p + 1 - recent.buf);
			if (!is_directory(git_path("rev-counter")))
				mkdir(git_path("rev-counter"), 0755);
			fd = open(git_path("rev-counter/recent"),
				  O_CREAT | O_TRUNC | O_RDWR, 0644);
			write_or_die(fd, recent.buf, recent.len);
			close(fd);
		}
	}

	do {
		commit_counter++;
		sprintf(buf, "%c%u", starter[run_counter], commit_counter);
	} while (!strbuf_find_commit_mark(&recent, buf, &oid));
}

void mark_commit(const struct object_id *oid)
{
	struct commit *c;
	char buf[64];
	char **val;

	if (latest_fd < 0)
		start_marking();

	if (!(c = lookup_commit(oid->hash)))
		return;

	update_commit_counter();
	sprintf(buf, "%s %u\n", oid_to_hex(oid), commit_counter);
	write_or_die(latest_fd, buf, strlen(buf));

	sprintf(buf, "%c%u", starter[run_counter], commit_counter);
	val = counter_at(&counter, c);
	*val = xstrdup(buf);
}

const char *oid_to_commit_mark(const struct object_id *oid)
{
	struct commit *c;
	char **val;

	if (!(c = lookup_commit(oid->hash)) ||
	    !(val = counter_peek(&counter, c)))
		return NULL;

	return *val;
}

int commit_mark_to_oid(const char *mark, struct object_id *oid)
{
	char buf[16];
	int ret, c;
	char r_ch;
	struct strbuf sb = STRBUF_INIT;

	if (sscanf(mark, "%c%u", &r_ch, &c) != 2 ||
	    !strchr(starter, r_ch) ||
	    strbuf_read_file(&sb, git_path("rev-counter/%c", r_ch), 0) < 0)
		return -1;

	sprintf(buf, "%u", c);
	ret = strbuf_find_commit_mark(&sb, buf, oid);
	strbuf_release(&sb);

	if (!ret) {
		char line[80];
		static int fd;

		if (!fd) {
			if (!is_directory(git_path("rev-counter")))
				mkdir(git_path("rev-counter"), 0755);
			fd = open(git_path("rev-counter/recent"),
				  O_CREAT | O_RDWR | O_APPEND, 0644);
			if (fd == -1)
				die("failed to open rev-counter/recent");
		}
		sprintf(line, "%s %s\n", oid_to_hex(oid), mark);
		write_or_die(fd, line, strlen(line));
	}
	return ret;
}
