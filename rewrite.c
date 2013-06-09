#include "cache.h"
#include "rewrite.h"
#include "run-command.h"
#include "notes-utils.h"
#include "builtin.h"

void add_rewritten(struct rewritten *list, unsigned char *from, unsigned char *to)
{
	struct rewritten_item *item;
	ALLOC_GROW(list->items, list->nr + 1, list->alloc);
	item = &list->items[list->nr];
	hashcpy(item->from, from);
	hashcpy(item->to, to);
	list->nr++;
}

int store_rewritten(struct rewritten *list, const char *file)
{
	static struct lock_file lock;
	struct strbuf buf = STRBUF_INIT;
	int fd, i, ret = 0;

	fd = hold_lock_file_for_update(&lock, file, LOCK_DIE_ON_ERROR);
	for (i = 0; i < list->nr; i++) {
		struct rewritten_item *item = &list->items[i];
		strbuf_addf(&buf, "%s %s\n", sha1_to_hex(item->from), sha1_to_hex(item->to));
	}
	if (write_in_full(fd, buf.buf, buf.len) < 0) {
		error(_("Could not write to %s"), file);
		ret = 1;
		goto leave;
	}
	if (commit_lock_file(&lock) < 0) {
		error(_("Error wrapping up %s."), file);
		ret = 1;
		goto leave;
	}
leave:
	strbuf_release(&buf);
	return ret;
}

void load_rewritten(struct rewritten *list, const char *file)
{
	struct strbuf buf = STRBUF_INIT;
	char *p;
	int fd;

	fd = open(file, O_RDONLY);
	if (fd < 0)
		return;
	if (strbuf_read(&buf, fd, 0) < 0) {
		close(fd);
		strbuf_release(&buf);
		return;
	}
	close(fd);

	for (p = buf.buf; *p;) {
		unsigned char from[20];
		unsigned char to[20];
		char *eol = strchrnul(p, '\n');
		if (eol - p != 81)
			/* wrong size */
			break;
		if (get_sha1_hex(p, from))
			break;
		if (get_sha1_hex(p + 41, to))
			break;
		add_rewritten(list, from, to);
		p = *eol ? eol + 1 : eol;
	}
	strbuf_release(&buf);
}

int run_rewrite_hook(struct rewritten *list, const char *name)
{
	struct strbuf buf = STRBUF_INIT;
	struct child_process proc;
	const char *argv[3];
	int code, i;

	argv[0] = find_hook("post-rewrite");
	if (!argv[0])
		return 0;

	argv[1] = name;
	argv[2] = NULL;

	memset(&proc, 0, sizeof(proc));
	proc.argv = argv;
	proc.in = -1;
	proc.stdout_to_stderr = 1;

	code = start_command(&proc);
	if (code)
		return code;
	for (i = 0; i < list->nr; i++) {
		struct rewritten_item *item = &list->items[i];
		strbuf_addf(&buf, "%s %s\n", sha1_to_hex(item->from), sha1_to_hex(item->to));
	}
	write_in_full(proc.in, buf.buf, buf.len);
	close(proc.in);
	return finish_command(&proc);
}

void copy_rewrite_notes(struct rewritten *list, const char *name, const char *msg)
{
	struct notes_rewrite_cfg *cfg;
	int i;

	cfg = init_copy_notes_for_rewrite(name);
	if (!cfg)
		return;

	for (i = 0; i < list->nr; i++) {
		struct rewritten_item *item = &list->items[i];
		copy_note_for_rewrite(cfg, item->from, item->to);
	}

	finish_copy_notes_for_rewrite(cfg, msg);
}
