#ifndef REBASE_TODO_H
#define REBASE_TODO_H

struct strbuf;

enum rebase_todo_action {
	REBASE_TODO_NONE = 0, /* comment or blank line */
	REBASE_TODO_NOOP,
	REBASE_TODO_DROP,
	REBASE_TODO_PICK,
	REBASE_TODO_REWORD,
	REBASE_TODO_EDIT,
	REBASE_TODO_SQUASH,
	REBASE_TODO_FIXUP,
	REBASE_TODO_EXEC
};

struct rebase_todo_item {
	enum rebase_todo_action action;
	char cmd[8];
	struct object_id oid;
	char *rest;
};

void rebase_todo_item_init(struct rebase_todo_item *);

void rebase_todo_item_release(struct rebase_todo_item *);

void rebase_todo_item_copy(struct rebase_todo_item *dst, const struct rebase_todo_item *src);

int rebase_todo_item_parse(struct rebase_todo_item *, const char *line, int strict);

void strbuf_add_rebase_todo_item(struct strbuf *, const struct rebase_todo_item *, int abbrev);

struct rebase_todo_list {
	struct rebase_todo_item *items;
	unsigned int nr;
	unsigned int alloc;
};

#define REBASE_TODO_LIST_INIT { NULL, 0, 0 }

void rebase_todo_list_init(struct rebase_todo_list *);

void rebase_todo_list_release(struct rebase_todo_list *);

void rebase_todo_list_clear(struct rebase_todo_list *);

void rebase_todo_list_swap(struct rebase_todo_list *dst, struct rebase_todo_list *src);

/* Push an empty line */
struct rebase_todo_item *rebase_todo_list_push_empty(struct rebase_todo_list *);

struct rebase_todo_item *rebase_todo_list_push(struct rebase_todo_list *, const struct rebase_todo_item *);

/* Push a noop command */
struct rebase_todo_item *rebase_todo_list_push_noop(struct rebase_todo_list *);

struct rebase_todo_item *rebase_todo_list_push_exec(struct rebase_todo_list *, const char *);

/* Push a comment */
struct rebase_todo_item *rebase_todo_list_push_comment(struct rebase_todo_list *, const char *comment);

__attribute__((format (printf, 2, 3)))
struct rebase_todo_item *rebase_todo_list_push_commentf(struct rebase_todo_list *, const char *fmt, ...);

unsigned int rebase_todo_list_count(const struct rebase_todo_list *);

int rebase_todo_list_load(struct rebase_todo_list *, const char *path, int strict);

void rebase_todo_list_save(const struct rebase_todo_list *, const char *path, unsigned int offset, int abbrev);

#endif /* REBASE_TODO_H */
