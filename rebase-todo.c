#include "cache.h"
#include "rebase-todo.h"

/*
 * Used as the default `rest` value, so that people can always assume `rest` is
 * non NULL and `rest` is NUL terminated even for a freshly initialized
 * rebase_todo_item.
 */
static char rebase_todo_item_slopbuf[1];

void rebase_todo_item_init(struct rebase_todo_item *item)
{
	item->action = REBASE_TODO_NONE;
	item->cmd[0] = '\0';
	oidclr(&item->oid);
	item->rest = rebase_todo_item_slopbuf;
}

void rebase_todo_item_release(struct rebase_todo_item *item)
{
	if (item->rest != rebase_todo_item_slopbuf)
		free(item->rest);
	rebase_todo_item_init(item);
}

void rebase_todo_item_copy(struct rebase_todo_item *dst, const struct rebase_todo_item *src)
{
	if (dst->rest != rebase_todo_item_slopbuf)
		free(dst->rest);
	*dst = *src;
	dst->rest = xstrdup(src->rest);
}

static const char *next_word(struct strbuf *sb, const char *str)
{
	const char *end;

	while (*str && isspace(*str))
		str++;

	end = str;
	while (*end && !isspace(*end))
		end++;

	strbuf_reset(sb);
	strbuf_add(sb, str, end - str);
	return end;
}

/*
 * NOTE: This combines transform_todo_ids() and check_bad_cmd_and_sha() as well
 */
int rebase_todo_item_parse(struct rebase_todo_item *item, const char *line, int strict)
{
	struct strbuf word = STRBUF_INIT;
	const char *str = line;
	int has_oid = 1, ret = 0;

	while (*str && isspace(*str))
		str++;

	if (!*str || *str == comment_line_char) {
		item->action = REBASE_TODO_NONE;
		xsnprintf(item->cmd, sizeof(item->cmd), "%c", *str);
		if (*str)
			str++;
		while (*str && isspace(*str))
			str++;
		if (item->rest != rebase_todo_item_slopbuf)
			free(item->rest);
		item->rest = *str ? xstrdup(str) : rebase_todo_item_slopbuf;
		return 0;
	}

	str = next_word(&word, str);
	if (!strcmp(word.buf, "noop")) {
		item->action = REBASE_TODO_NOOP;
		has_oid = 0;
	} else if (!strcmp(word.buf, "drop") || !strcmp(word.buf, "d"))
		item->action = REBASE_TODO_DROP;
	else if (!strcmp(word.buf, "pick") || !strcmp(word.buf, "p"))
		item->action = REBASE_TODO_PICK;
	else if (!strcmp(word.buf, "reword") || !strcmp(word.buf, "r"))
		item->action = REBASE_TODO_REWORD;
	else if (!strcmp(word.buf, "edit") || !strcmp(word.buf, "e"))
		item->action = REBASE_TODO_EDIT;
	else if (!strcmp(word.buf, "squash") || !strcmp(word.buf, "s"))
		item->action = REBASE_TODO_SQUASH;
	else if (!strcmp(word.buf, "fixup") || !strcmp(word.buf, "f"))
		item->action = REBASE_TODO_FIXUP;
	else if (!strcmp(word.buf, "exec") || !strcmp(word.buf, "x")) {
		item->action = REBASE_TODO_EXEC;
		has_oid = 0;
	} else {
		ret = error(_("Unknown command: %s"), word.buf);
		goto finish;
	}
	xsnprintf(item->cmd, sizeof(item->cmd), "%s", word.buf);

	if (has_oid) {
		str = next_word(&word, str);
		if (strict) {
			if (word.len != GIT_SHA1_HEXSZ || get_oid_hex(word.buf, &item->oid)) {
				ret = error(_("Invalid line: %s"), line);
				goto finish;
			}
		} else {
			if (get_sha1_commit(word.buf, item->oid.hash)) {
				ret = error(_("Not a commit: %s"), word.buf);
				goto finish;
			}
		}
	} else {
		oidclr(&item->oid);
	}

	if (*str && isspace(*str))
		str++;
	if (*str) {
		if (item->rest != rebase_todo_item_slopbuf)
			free(item->rest);
		item->rest = xstrdup(str);
	}

finish:
	strbuf_release(&word);
	return ret;
}

void strbuf_add_rebase_todo_item(struct strbuf *sb, const struct rebase_todo_item *item, int abbrev)
{
	if (*item->cmd)
		strbuf_addstr(sb, item->cmd);

	if (!is_null_oid(&item->oid)) {
		strbuf_addch(sb, ' ');
		if (abbrev)
			strbuf_addstr(sb, find_unique_abbrev((unsigned char *)&item->oid.hash, DEFAULT_ABBREV));
		else
			strbuf_addstr(sb, oid_to_hex(&item->oid));
	}

	if (*item->rest) {
		if (*item->cmd)
			strbuf_addch(sb, ' ');
		strbuf_addstr(sb, item->rest);
	}

	strbuf_addch(sb, '\n');
}

void rebase_todo_list_init(struct rebase_todo_list *list)
{
	list->items = NULL;
	list->nr = 0;
	list->alloc = 0;
}

void rebase_todo_list_release(struct rebase_todo_list *list)
{
	rebase_todo_list_clear(list);
	free(list->items);
	rebase_todo_list_init(list);
}

void rebase_todo_list_clear(struct rebase_todo_list *list)
{
	unsigned int i;
	for (i = 0; i < list->nr; i++)
		rebase_todo_item_release(&list->items[i]);
	list->nr = 0;
}

void rebase_todo_list_swap(struct rebase_todo_list *dst, struct rebase_todo_list *src)
{
	struct rebase_todo_list tmp = *dst;
	*dst = *src;
	*src = tmp;
}

struct rebase_todo_item *rebase_todo_list_push_empty(struct rebase_todo_list *list)
{
	struct rebase_todo_item *item;

	ALLOC_GROW(list->items, list->nr + 1, list->alloc);
	item = &list->items[list->nr++];
	rebase_todo_item_init(item);
	return item;
}

struct rebase_todo_item *rebase_todo_list_push(struct rebase_todo_list *list, const struct rebase_todo_item *src_item)
{
	struct rebase_todo_item *item = rebase_todo_list_push_empty(list);
	rebase_todo_item_copy(item, src_item);
	return item;
}

struct rebase_todo_item *rebase_todo_list_push_noop(struct rebase_todo_list *list)
{
	struct rebase_todo_item *item = rebase_todo_list_push_empty(list);
	item->action = REBASE_TODO_NOOP;
	xsnprintf(item->cmd, sizeof(item->cmd), "noop");
	return item;
}

struct rebase_todo_item *rebase_todo_list_push_exec(struct rebase_todo_list *list, const char *cmd)
{
	struct rebase_todo_item *item = rebase_todo_list_push_empty(list);
	item->action = REBASE_TODO_EXEC;
	xsnprintf(item->cmd, sizeof(item->cmd), "exec");
	item->rest = xstrdup(cmd);
	return item;
}

struct rebase_todo_item *rebase_todo_list_push_comment(struct rebase_todo_list *list, const char *comment)
{
	struct rebase_todo_item *item = rebase_todo_list_push_empty(list);
	struct strbuf sb = STRBUF_INIT;

	strbuf_add_commented_lines(&sb, comment, strlen(comment));
	if (sb.len && sb.buf[sb.len-1] == '\n')
		strbuf_setlen(&sb, sb.len - 1);
	item->rest = strbuf_detach(&sb, NULL);
	return item;
}

struct rebase_todo_item *rebase_todo_list_push_commentf(struct rebase_todo_list *list, const char *fmt, ...)
{
	va_list ap;
	struct rebase_todo_item *item;
	struct strbuf sb = STRBUF_INIT;

	va_start(ap, fmt);
	strbuf_vaddf(&sb, fmt, ap);
	va_end(ap);

	item = rebase_todo_list_push_comment(list, sb.buf);
	strbuf_release(&sb);
	return item;
}

unsigned int rebase_todo_list_count(const struct rebase_todo_list *list)
{
	unsigned int i, count = 0;

	for (i = 0; i < list->nr; i++)
		if (list->items[i].action)
			count++;

	return count;
}

int rebase_todo_list_load(struct rebase_todo_list *list, const char *path, int strict)
{
	struct strbuf sb = STRBUF_INIT;
	FILE *fp;

	fp = fopen(path, "r");
	if (!fp)
		return error(_("could not open %s for reading"), path);

	while (strbuf_getline(&sb, fp) != EOF) {
		struct rebase_todo_item *item = rebase_todo_list_push_empty(list);
		if (rebase_todo_item_parse(item, sb.buf, strict) < 0) {
			strbuf_release(&sb);
			fclose(fp);
			return -1;
		}
	}
	strbuf_release(&sb);
	fclose(fp);
	return 0;
}

void rebase_todo_list_save(const struct rebase_todo_list *list, const char *filename, unsigned int offset, int abbrev)
{
	char *tmpfile = mkpathdup("%s.new", filename);
	struct strbuf sb = STRBUF_INIT;
	int fd;

	for (; offset < list->nr; offset++)
		strbuf_add_rebase_todo_item(&sb, &list->items[offset], abbrev);

	fd = xopen(tmpfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (write_in_full(fd, sb.buf, sb.len) != sb.len)
		die_errno(_("could not write to %s"), tmpfile);
	close(fd);
	strbuf_release(&sb);

	if (rename(tmpfile, filename))
		die_errno(_("rename failed"));

	free(tmpfile);
}
