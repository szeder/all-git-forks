#ifndef REBASE_TODO_H
#define REBASE_TODO_H

struct strbuf;

enum rebase_todo_action {
	REBASE_TODO_NONE = 0,
	REBASE_TODO_NOOP,
	REBASE_TODO_PICK
};

struct rebase_todo_item {
	enum rebase_todo_action action;
	struct object_id oid;
	char *rest;
};

void rebase_todo_item_init(struct rebase_todo_item *);

void rebase_todo_item_release(struct rebase_todo_item *);

void rebase_todo_item_copy(struct rebase_todo_item *, const struct rebase_todo_item *);

int rebase_todo_item_parse(struct rebase_todo_item *, const char *line, int abbrev);

void strbuf_add_rebase_todo_item(struct strbuf *, const struct rebase_todo_item *, int abbrev);

#endif /* REBASE_TODO_H */
