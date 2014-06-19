#include "cache.h"
#include "string-list.h"
#include "run-command.h"
#include "string-list.h"
#include "trailer.h"
/*
 * Copyright (c) 2013, 2014 Christian Couder <chriscool@tuxfamily.org>
 */

enum action_where { WHERE_AFTER, WHERE_BEFORE };
enum action_if_exists { EXISTS_ADD_IF_DIFFERENT, EXISTS_ADD_IF_DIFFERENT_NEIGHBOR,
			EXISTS_ADD, EXISTS_OVERWRITE, EXISTS_DO_NOTHING };
enum action_if_missing { MISSING_ADD, MISSING_DO_NOTHING };

struct conf_info {
	char *name;
	char *key;
	char *command;
	unsigned command_uses_arg : 1;
	enum action_where where;
	enum action_if_exists if_exists;
	enum action_if_missing if_missing;
};

#define TRAILER_ARG_STRING "$ARG"

struct trailer_item {
	struct trailer_item *previous;
	struct trailer_item *next;
	const char *token;
	const char *value;
	struct conf_info conf;
};

static struct trailer_item *first_conf_item;

static int same_token(struct trailer_item *a, struct trailer_item *b, int alnum_len)
{
	return !strncasecmp(a->token, b->token, alnum_len);
}

static int same_value(struct trailer_item *a, struct trailer_item *b)
{
	return !strcasecmp(a->value, b->value);
}

static int same_trailer(struct trailer_item *a, struct trailer_item *b, int alnum_len)
{
	return same_token(a, b, alnum_len) && same_value(a, b);
}

/* Get the length of buf from its beginning until its last alphanumeric character */
static size_t alnum_len(const char *buf, size_t len)
{
	while (len > 0 && !isalnum(buf[len - 1]))
		len--;
	return len;
}

static inline int contains_only_spaces(const char *str)
{
	const char *s = str;
	while (*s && isspace(*s))
		s++;
	return !*s;
}

static inline void strbuf_replace(struct strbuf *sb, const char *a, const char *b)
{
	const char *ptr = strstr(sb->buf, a);
	if (ptr)
		strbuf_splice(sb, ptr - sb->buf, strlen(a), b, strlen(b));
}

static void free_trailer_item(struct trailer_item *item)
{
	free(item->conf.name);
	free(item->conf.key);
	free(item->conf.command);
	free((char *)item->token);
	free((char *)item->value);
	free(item);
}

static void print_tok_val(const char *tok, const char *val)
{
	char c = tok[strlen(tok) - 1];
	if (isalnum(c))
		printf("%s: %s\n", tok, val);
	else
		printf("%s%s\n", tok, val);
}

static void print_all(struct trailer_item *first, int trim_empty)
{
	struct trailer_item *item;
	for (item = first; item; item = item->next) {
		if (!trim_empty || strlen(item->value) > 0)
			print_tok_val(item->token, item->value);
	}
}

static void add_arg_to_input_list(struct trailer_item *in_tok,
				  struct trailer_item *arg_tok)
{
	if (arg_tok->conf.where == WHERE_AFTER) {
		arg_tok->next = in_tok->next;
		in_tok->next = arg_tok;
		arg_tok->previous = in_tok;
		if (arg_tok->next)
			arg_tok->next->previous = arg_tok;
	} else {
		arg_tok->previous = in_tok->previous;
		in_tok->previous = arg_tok;
		arg_tok->next = in_tok;
		if (arg_tok->previous)
			arg_tok->previous->next = arg_tok;
	}
}

static int check_if_different(struct trailer_item *in_tok,
			      struct trailer_item *arg_tok,
			      int alnum_len, int check_all)
{
	enum action_where where = arg_tok->conf.where;
	do {
		if (!in_tok)
			return 1;
		if (same_trailer(in_tok, arg_tok, alnum_len))
			return 0;
		/*
		 * if we want to add a trailer after another one,
		 * we have to check those before this one
		 */
		in_tok = (where == WHERE_AFTER) ? in_tok->previous : in_tok->next;
	} while (check_all);
	return 1;
}

static void apply_arg_if_exists(struct trailer_item *in_tok,
				struct trailer_item *arg_tok,
				int alnum_len)
{
	switch (arg_tok->conf.if_exists) {
	case EXISTS_DO_NOTHING:
		free_trailer_item(arg_tok);
		break;
	case EXISTS_OVERWRITE:
		free((char *)in_tok->value);
		in_tok->value = xstrdup(arg_tok->value);
		free_trailer_item(arg_tok);
		break;
	case EXISTS_ADD:
		add_arg_to_input_list(in_tok, arg_tok);
		break;
	case EXISTS_ADD_IF_DIFFERENT:
		if (check_if_different(in_tok, arg_tok, alnum_len, 1))
			add_arg_to_input_list(in_tok, arg_tok);
		else
			free_trailer_item(arg_tok);
		break;
	case EXISTS_ADD_IF_DIFFERENT_NEIGHBOR:
		if (check_if_different(in_tok, arg_tok, alnum_len, 0))
			add_arg_to_input_list(in_tok, arg_tok);
		else
			free_trailer_item(arg_tok);
		break;
	}
}

static void remove_from_list(struct trailer_item *item,
			     struct trailer_item **first)
{
	if (item->next)
		item->next->previous = item->previous;
	if (item->previous)
		item->previous->next = item->next;
	else
		*first = item->next;
}

static struct trailer_item *remove_first(struct trailer_item **first)
{
	struct trailer_item *item = *first;
	*first = item->next;
	if (item->next) {
		item->next->previous = NULL;
		item->next = NULL;
	}
	return item;
}

static void process_input_token(struct trailer_item *in_tok,
				struct trailer_item **arg_tok_first,
				enum action_where where)
{
	struct trailer_item *arg_tok;
	struct trailer_item *next_arg;

	int after = where == WHERE_AFTER;
	int tok_alnum_len = alnum_len(in_tok->token, strlen(in_tok->token));

	for (arg_tok = *arg_tok_first; arg_tok; arg_tok = next_arg) {
		next_arg = arg_tok->next;
		if (!same_token(in_tok, arg_tok, tok_alnum_len))
			continue;
		if (arg_tok->conf.where != where)
			continue;
		remove_from_list(arg_tok, arg_tok_first);
		apply_arg_if_exists(in_tok, arg_tok, tok_alnum_len);
		/*
		 * If arg has been added to input,
		 * then we need to process it too now.
		 */
		if ((after ? in_tok->next : in_tok->previous) == arg_tok)
			in_tok = arg_tok;
	}
}

static void update_last(struct trailer_item **last)
{
	if (*last)
		while ((*last)->next != NULL)
			*last = (*last)->next;
}

static void update_first(struct trailer_item **first)
{
	if (*first)
		while ((*first)->previous != NULL)
			*first = (*first)->previous;
}

static void apply_arg_if_missing(struct trailer_item **in_tok_first,
				 struct trailer_item **in_tok_last,
				 struct trailer_item *arg_tok)
{
	struct trailer_item **in_tok;
	enum action_where where;

	switch (arg_tok->conf.if_missing) {
	case MISSING_DO_NOTHING:
		free_trailer_item(arg_tok);
		break;
	case MISSING_ADD:
		where = arg_tok->conf.where;
		in_tok = (where == WHERE_AFTER) ? in_tok_last : in_tok_first;
		if (*in_tok) {
			add_arg_to_input_list(*in_tok, arg_tok);
			*in_tok = arg_tok;
		} else {
			*in_tok_first = arg_tok;
			*in_tok_last = arg_tok;
		}
		break;
	}
}

static void process_trailers_lists(struct trailer_item **in_tok_first,
				   struct trailer_item **in_tok_last,
				   struct trailer_item **arg_tok_first)
{
	struct trailer_item *in_tok;
	struct trailer_item *arg_tok;

	if (!*arg_tok_first)
		return;

	/* Process input from end to start */
	for (in_tok = *in_tok_last; in_tok; in_tok = in_tok->previous)
		process_input_token(in_tok, arg_tok_first, WHERE_AFTER);

	update_last(in_tok_last);

	if (!*arg_tok_first)
		return;

	/* Process input from start to end */
	for (in_tok = *in_tok_first; in_tok; in_tok = in_tok->next)
		process_input_token(in_tok, arg_tok_first, WHERE_BEFORE);

	update_first(in_tok_first);

	/* Process args left */
	while (*arg_tok_first) {
		arg_tok = remove_first(arg_tok_first);
		apply_arg_if_missing(in_tok_first, in_tok_last, arg_tok);
	}
}

static int set_where(struct conf_info *item, const char *value)
{
	if (!strcasecmp("after", value))
		item->where = WHERE_AFTER;
	else if (!strcasecmp("before", value))
		item->where = WHERE_BEFORE;
	else
		return -1;
	return 0;
}

static int set_if_exists(struct conf_info *item, const char *value)
{
	if (!strcasecmp("addIfDifferent", value))
		item->if_exists = EXISTS_ADD_IF_DIFFERENT;
	else if (!strcasecmp("addIfDifferentNeighbor", value))
		item->if_exists = EXISTS_ADD_IF_DIFFERENT_NEIGHBOR;
	else if (!strcasecmp("add", value))
		item->if_exists = EXISTS_ADD;
	else if (!strcasecmp("overwrite", value))
		item->if_exists = EXISTS_OVERWRITE;
	else if (!strcasecmp("doNothing", value))
		item->if_exists = EXISTS_DO_NOTHING;
	else
		return -1;
	return 0;
}

static int set_if_missing(struct conf_info *item, const char *value)
{
	if (!strcasecmp("doNothing", value))
		item->if_missing = MISSING_DO_NOTHING;
	else if (!strcasecmp("add", value))
		item->if_missing = MISSING_ADD;
	else
		return -1;
	return 0;
}

static struct trailer_item *get_conf_item(const char *name)
{
	struct trailer_item *item;
	struct trailer_item *previous;

	/* Look up item with same name */
	for (previous = NULL, item = first_conf_item;
	     item;
	     previous = item, item = item->next) {
		if (!strcasecmp(item->conf.name, name))
			return item;
	}

	/* Item does not already exists, create it */
	item = xcalloc(sizeof(struct trailer_item), 1);
	item->conf.name = xstrdup(name);

	if (!previous)
		first_conf_item = item;
	else {
		previous->next = item;
		item->previous = previous;
	}

	return item;
}

enum trailer_info_type { TRAILER_KEY, TRAILER_COMMAND, TRAILER_WHERE,
			 TRAILER_IF_EXISTS, TRAILER_IF_MISSING };

static struct {
	const char *name;
	enum trailer_info_type type;
} trailer_config_items[] = {
	{ "key", TRAILER_KEY },
	{ "command", TRAILER_COMMAND },
	{ "where", TRAILER_WHERE },
	{ "ifexists", TRAILER_IF_EXISTS },
	{ "ifmissing", TRAILER_IF_MISSING }
};

static int git_trailer_config(const char *conf_key, const char *value, void *cb)
{
	const char *trailer_item, *variable_name;
	struct trailer_item *item;
	struct conf_info *conf;
	char *name = NULL;
	enum trailer_info_type type;
	int i;

	if (!skip_prefix(conf_key, "trailer.", &trailer_item))
		return 0;

	variable_name = strrchr(trailer_item, '.');
	if (!variable_name) {
		warning(_("two level trailer config variable %s"), conf_key);
		return 0;
	}

	variable_name++;
	for (i = 0; i < ARRAY_SIZE(trailer_config_items); i++) {
		if (strcmp(trailer_config_items[i].name, variable_name))
			continue;
		name = xstrndup(trailer_item,  variable_name - trailer_item - 1);
		type = trailer_config_items[i].type;
		break;
	}

	if (!name)
		return 0;

	item = get_conf_item(name);
	conf = &item->conf;
	free(name);

	switch (type) {
	case TRAILER_KEY:
		if (conf->key)
			warning(_("more than one %s"), conf_key);
		conf->key = xstrdup(value);
		break;
	case TRAILER_COMMAND:
		if (conf->command)
			warning(_("more than one %s"), conf_key);
		conf->command = xstrdup(value);
		conf->command_uses_arg = !!strstr(conf->command, TRAILER_ARG_STRING);
		break;
	case TRAILER_WHERE:
		if (set_where(conf, value))
			warning(_("unknown value '%s' for key '%s'"), value, conf_key);
		break;
	case TRAILER_IF_EXISTS:
		if (set_if_exists(conf, value))
			warning(_("unknown value '%s' for key '%s'"), value, conf_key);
		break;
	case TRAILER_IF_MISSING:
		if (set_if_missing(conf, value))
			warning(_("unknown value '%s' for key '%s'"), value, conf_key);
		break;
	default:
		die("internal bug in trailer.c");
	}
	return 0;
}

static int parse_trailer(struct strbuf *tok, struct strbuf *val, const char *trailer)
{
	size_t len = strcspn(trailer, "=:");
	if (len == 0)
		return error(_("empty trailer token in trailer '%s'"), trailer);
	if (len < strlen(trailer)) {
		strbuf_add(tok, trailer, len);
		strbuf_trim(tok);
		strbuf_addstr(val, trailer + len + 1);
		strbuf_trim(val);
	} else {
		strbuf_addstr(tok, trailer);
		strbuf_trim(tok);
	}
	return 0;
}

static int read_from_command(struct child_process *cp, struct strbuf *buf)
{
	if (run_command(cp))
		return error("running trailer command '%s' failed", cp->argv[0]);
	if (strbuf_read(buf, cp->out, 1024) < 1)
		return error("reading from trailer command '%s' failed", cp->argv[0]);
	strbuf_trim(buf);
	return 0;
}

static const char *apply_command(const char *command, const char *arg)
{
	struct strbuf cmd = STRBUF_INIT;
	struct strbuf buf = STRBUF_INIT;
	struct child_process cp;
	const char *argv[] = {NULL, NULL};
	const char *result;

	strbuf_addstr(&cmd, command);
	if (arg)
		strbuf_replace(&cmd, TRAILER_ARG_STRING, arg);

	argv[0] = cmd.buf;
	memset(&cp, 0, sizeof(cp));
	cp.argv = argv;
	cp.env = local_repo_env;
	cp.no_stdin = 1;
	cp.out = -1;
	cp.use_shell = 1;

	if (read_from_command(&cp, &buf)) {
		strbuf_release(&buf);
		result = xstrdup("");
	} else
		result = strbuf_detach(&buf, NULL);

	strbuf_release(&cmd);
	return result;
}

static void duplicate_conf(struct conf_info *dst, struct conf_info *src)
{
	*dst = *src;
	if (src->name)
		dst->name = xstrdup(src->name);
	if (src->key)
		dst->key = xstrdup(src->key);
	if (src->command)
		dst->command = xstrdup(src->command);
}

static const char *token_from_item(struct trailer_item *item)
{
	if (item->conf.key)
		return item->conf.key;

	return item->conf.name;
}

static struct trailer_item *new_trailer_item(struct trailer_item *conf_item,
					     char *tok, char *val)
{
	struct trailer_item *new = xcalloc(sizeof(*new), 1);
	new->value = val;

	if (conf_item) {
		duplicate_conf(&new->conf, &conf_item->conf);
		new->token = xstrdup(token_from_item(conf_item));
		free(tok);
		if (conf_item->conf.command_uses_arg || !val) {
			new->value = apply_command(conf_item->conf.command, val);
			free(val);
		}
	} else
		new->token = tok;

	return new;
}

static int token_matches_item(const char *tok, struct trailer_item *item, int alnum_len)
{
	if (!strncasecmp(tok, item->conf.name, alnum_len))
		return 1;
	return item->conf.key ? !strncasecmp(tok, item->conf.key, alnum_len) : 0;
}

static struct trailer_item *create_trailer_item(const char *string)
{
	struct strbuf tok = STRBUF_INIT;
	struct strbuf val = STRBUF_INIT;
	struct trailer_item *item;
	int tok_alnum_len;

	if (parse_trailer(&tok, &val, string))
		return NULL;

	tok_alnum_len = alnum_len(tok.buf, tok.len);

	/* Lookup if the token matches something in the config */
	for (item = first_conf_item; item; item = item->next) {
		if (token_matches_item(tok.buf, item, tok_alnum_len)) {
			strbuf_release(&tok);
			return new_trailer_item(item,
						NULL,
						strbuf_detach(&val, NULL));
		}
	}

	return new_trailer_item(NULL,
				strbuf_detach(&tok, NULL),
				strbuf_detach(&val, NULL));
}

static void add_trailer_item(struct trailer_item **first,
			     struct trailer_item **last,
			     struct trailer_item *new)
{
	if (!new)
		return;
	if (!*last) {
		*first = new;
		*last = new;
	} else {
		(*last)->next = new;
		new->previous = *last;
		*last = new;
	}
}

static struct trailer_item *process_command_line_args(struct string_list *trailers)
{
	struct trailer_item *arg_tok_first = NULL;
	struct trailer_item *arg_tok_last = NULL;
	struct string_list_item *tr;
	struct trailer_item *item;

	for_each_string_list_item(tr, trailers) {
		struct trailer_item *new = create_trailer_item(tr->string);
		add_trailer_item(&arg_tok_first, &arg_tok_last, new);
	}

	/* Add conf commands that don't use $ARG */
	for (item = first_conf_item; item; item = item->next) {
		if (item->conf.command && !item->conf.command_uses_arg) {
			struct trailer_item *new = new_trailer_item(item, NULL, NULL);
			add_trailer_item(&arg_tok_first, &arg_tok_last, new);
		}
	}

	return arg_tok_first;
}

static struct strbuf **read_input_file(const char *file)
{
	struct strbuf **lines;
	struct strbuf sb = STRBUF_INIT;

	if (file) {
		if (strbuf_read_file(&sb, file, 0) < 0)
			die_errno(_("could not read input file '%s'"), file);
	} else {
		if (strbuf_read(&sb, fileno(stdin), 0) < 0)
			die_errno(_("could not read from stdin"));
	}

	lines = strbuf_split(&sb, '\n');

	strbuf_release(&sb);

	return lines;
}

/*
 * Return the (0 based) index of the start of the patch or the line
 * count if there is no patch in the message.
 */
static int find_patch_start(struct strbuf **lines, int count)
{
	int i;

	/* Get the start of the patch part if any */
	for (i = 0; i < count; i++) {
		if (starts_with(lines[i]->buf, "---"))
			return i;
	}

	return count;
}

/*
 * Return the (0 based) index of the first trailer line or count if
 * there are no trailers. Trailers are searched only in the lines from
 * index (count - 1) down to index 0.
 */
static int find_trailer_start(struct strbuf **lines, int count)
{
	int start, only_spaces = 1;

	/*
	 * Get the start of the trailers by looking starting from the end
	 * for a line with only spaces before lines with one ':'.
	 */
	for (start = count - 1; start >= 0; start--) {
		if (lines[start]->buf[0] == comment_line_char)
			continue;
		if (contains_only_spaces(lines[start]->buf)) {
			if (only_spaces)
				continue;
			return start + 1;
		}
		if (strchr(lines[start]->buf, ':')) {
			if (only_spaces)
				only_spaces = 0;
			continue;
		}
		return count;
	}

	return only_spaces ? count : 0;
}

static int has_blank_line_before(struct strbuf **lines, int start)
{
	for (;start >= 0; start--) {
		if (lines[start]->buf[0] == comment_line_char)
			continue;
		return contains_only_spaces(lines[start]->buf);
	}
	return 0;
}

static void print_lines(struct strbuf **lines, int start, int end)
{
	int i;
	for (i = start; lines[i] && i < end; i++)
		printf("%s", lines[i]->buf);
}

static int process_input_file(struct strbuf **lines,
			      struct trailer_item **in_tok_first,
			      struct trailer_item **in_tok_last)
{
	int count = 0;
	int patch_start, trailer_start, i;

	/* Get the line count */
	while (lines[count])
		count++;

	patch_start = find_patch_start(lines, count);
	trailer_start = find_trailer_start(lines, patch_start);

	/* Print lines before the trailers as is */
	print_lines(lines, 0, trailer_start);

	if (!has_blank_line_before(lines, trailer_start - 1))
		printf("\n");

	/* Parse trailer lines */
	for (i = trailer_start; i < patch_start; i++) {
		struct trailer_item *new = create_trailer_item(lines[i]->buf);
		add_trailer_item(in_tok_first, in_tok_last, new);
	}

	return patch_start;
}

static void free_all(struct trailer_item **first)
{
	while (*first) {
		struct trailer_item *item = remove_first(first);
		free_trailer_item(item);
	}
}

void process_trailers(const char *file, int trim_empty, struct string_list *trailers)
{
	struct trailer_item *in_tok_first = NULL;
	struct trailer_item *in_tok_last = NULL;
	struct trailer_item *arg_tok_first;
	struct strbuf **lines;
	int patch_start;

	git_config(git_trailer_config, NULL);

	lines = read_input_file(file);

	/* Print the lines before the trailers */
	patch_start = process_input_file(lines, &in_tok_first, &in_tok_last);

	arg_tok_first = process_command_line_args(trailers);

	process_trailers_lists(&in_tok_first, &in_tok_last, &arg_tok_first);

	print_all(in_tok_first, trim_empty);

	free_all(&in_tok_first);

	/* Print the lines after the trailers as is */
	print_lines(lines, patch_start, INT_MAX);

	strbuf_list_free(lines);
}
