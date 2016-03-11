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

struct rebase_todo_list {
	struct rebase_todo_item *items;
	unsigned int nr, alloc;
};

#define REBASE_TODO_LIST_INIT { NULL, 0, 0 }

void rebase_todo_list_init(struct rebase_todo_list *);

void rebase_todo_list_clear(struct rebase_todo_list *);

void rebase_todo_list_swap(struct rebase_todo_list *dst, struct rebase_todo_list *src);

unsigned int rebase_todo_list_count(const struct rebase_todo_list *);

struct rebase_todo_item *rebase_todo_list_push(struct rebase_todo_list *,
					       const struct rebase_todo_item *);

struct rebase_todo_item *rebase_todo_list_push_empty(struct rebase_todo_list *);

struct rebase_todo_item *rebase_todo_list_push_noop(struct rebase_todo_list *);

int rebase_todo_list_load(struct rebase_todo_list *, const char *path, int abbrev);

void rebase_todo_list_save(const struct rebase_todo_list *, const char *path,
			   unsigned int offset, int abbrev);

#endif /* REBASE_TODO_H */
