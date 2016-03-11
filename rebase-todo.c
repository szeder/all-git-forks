#include "cache.h"
#include "rebase-todo.h"

/*
 * Used as the default `rest` value, so that users can always assume `rest` is
 * non NULL and `rest` is NUL terminated even for a freshly initialized
 * rebase_todo_item.
 */
static char rebase_todo_item_slopbuf[1];

void rebase_todo_item_init(struct rebase_todo_item *item)
{
	item->action = REBASE_TODO_NONE;
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

int rebase_todo_item_parse(struct rebase_todo_item *item, const char *line, int abbrev)
{
	struct strbuf word = STRBUF_INIT;
	const char *str = line;
	int has_oid = 1, ret = 0;

	while (*str && isspace(*str))
		str++;

	if (!*str || *str == comment_line_char) {
		item->action = REBASE_TODO_NONE;
		oidclr(&item->oid);
		if (item->rest != rebase_todo_item_slopbuf)
			free(item->rest);
		item->rest = *str ? xstrdup(str) : rebase_todo_item_slopbuf;
		return 0;
	}

	str = next_word(&word, str);
	if (!strcmp(word.buf, "noop")) {
		item->action = REBASE_TODO_NOOP;
		has_oid = 0;
	} else if (!strcmp(word.buf, "pick") || !strcmp(word.buf, "p")) {
		item->action = REBASE_TODO_PICK;
	} else {
		ret = error(_("Unknown command: %s"), word.buf);
		goto finish;
	}

	if (has_oid) {
		str = next_word(&word, str);
		if (abbrev) {
			/* accept abbreviated object ids */
			if (get_oid_commit(word.buf, &item->oid)) {
				ret = error(_("Not a commit: %s"), word.buf);
				goto finish;
			}
		} else {
			if (word.len != GIT_SHA1_HEXSZ || get_oid_hex(word.buf, &item->oid)) {
				ret = error(_("Invalid line: %s"), line);
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

void strbuf_add_rebase_todo_item(struct strbuf *sb,
				 const struct rebase_todo_item *item, int abbrev)
{
	int has_oid = 1;

	switch (item->action) {
	case REBASE_TODO_NONE:
		has_oid = 0;
		break;
	case REBASE_TODO_NOOP:
		strbuf_addstr(sb, "noop");
		has_oid = 0;
		break;
	case REBASE_TODO_PICK:
		strbuf_addstr(sb, "pick");
		break;
	default:
		die("BUG: invalid rebase_todo_item action %d", item->action);
	}

	if (has_oid) {
		strbuf_addch(sb, ' ');
		if (abbrev)
			strbuf_addstr(sb, find_unique_abbrev((unsigned char *)&item->oid.hash, DEFAULT_ABBREV));
		else
			strbuf_addstr(sb, oid_to_hex(&item->oid));
	}

	if (*item->rest) {
		if (item->action != REBASE_TODO_NONE)
			strbuf_addch(sb, ' ');
		strbuf_addstr(sb, item->rest);
	}

	strbuf_addch(sb, '\n');
}
