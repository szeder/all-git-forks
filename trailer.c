#include "cache.h"
#include "run-command.h"
/*
 * Copyright (c) 2013 Christian Couder <chriscool@tuxfamily.org>
 */

enum action_where { WHERE_AFTER, WHERE_BEFORE };
enum action_if_exist { EXIST_ADD_IF_DIFFERENT, EXIST_ADD_IF_DIFFERENT_NEIGHBOR,
		       EXIST_ADD, EXIST_OVERWRITE, EXIST_DO_NOTHING };
enum action_if_missing { MISSING_ADD, MISSING_DO_NOTHING };

struct conf_info {
	char *name;
	char *key;
	char *command;
	unsigned command_uses_arg : 1;
	enum action_where where;
	enum action_if_exist if_exist;
	enum action_if_missing if_missing;
};

#define TRAILER_ARG_STRING "$ARG"

struct trailer_item {
	struct trailer_item *previous;
	struct trailer_item *next;
	const char *token;
	const char *value;
	struct conf_info *conf;
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
static size_t alnum_len(const char *buf, size_t len) {
	while (--len >= 0 && !isalnum(buf[len]));
	return len + 1;
}

static void print_tok_val(const char *tok, const char *val)
{
	char c = tok[strlen(tok) - 1];
	if (isalnum(c))
		printf("%s: %s\n", tok, val);
	else if (isspace(c) || c == '#')
		printf("%s%s\n", tok, val);
	else
		printf("%s %s\n", tok, val);
}

static void print_all(struct trailer_item *first, int trim_empty)
{
	struct trailer_item *item;
	for (item = first; item; item = item->next) {
		if (!trim_empty || strlen(item->value) > 0)
			print_tok_val(item->token, item->value);
	}
}

static void add_arg_to_infile(struct trailer_item *infile_tok,
			      struct trailer_item *arg_tok)
{
	if (arg_tok->conf->where == WHERE_AFTER) {
		arg_tok->next = infile_tok->next;
		infile_tok->next = arg_tok;
		arg_tok->previous = infile_tok;
		if (arg_tok->next)
			arg_tok->next->previous = arg_tok;
	} else {
		arg_tok->previous = infile_tok->previous;
		infile_tok->previous = arg_tok;
		arg_tok->next = infile_tok;
		if (arg_tok->previous)
			arg_tok->previous->next = arg_tok;
	}
}

static int check_if_different(struct trailer_item *infile_tok,
			      struct trailer_item *arg_tok,
			      int alnum_len, int check_all)
{
	enum action_where where = arg_tok->conf->where;
	do {
		if (!infile_tok)
			return 1;
		if (same_trailer(infile_tok, arg_tok, alnum_len))
			return 0;
		/*
		 * if we want to add a trailer after another one,
		 * we have to check those before this one
		 */
		infile_tok = (where == WHERE_AFTER) ? infile_tok->previous : infile_tok->next;
	} while (check_all);
	return 1;
}

static void apply_arg_if_exist(struct trailer_item *infile_tok,
			       struct trailer_item *arg_tok,
			       int alnum_len)
{
	switch(arg_tok->conf->if_exist) {
	case EXIST_DO_NOTHING:
		free(arg_tok);
		break;
	case EXIST_OVERWRITE:
		free((char *)infile_tok->value);
		infile_tok->value = xstrdup(arg_tok->value);
		free(arg_tok);
		break;
	case EXIST_ADD:
		add_arg_to_infile(infile_tok, arg_tok);
		break;
	case EXIST_ADD_IF_DIFFERENT:
		if (check_if_different(infile_tok, arg_tok, alnum_len, 1))
			add_arg_to_infile(infile_tok, arg_tok);
		else
			free(arg_tok);
		break;
	case EXIST_ADD_IF_DIFFERENT_NEIGHBOR:
		if (check_if_different(infile_tok, arg_tok, alnum_len, 0))
			add_arg_to_infile(infile_tok, arg_tok);
		else
			free(arg_tok);
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

static void process_infile_tok(struct trailer_item *infile_tok,
			       struct trailer_item **arg_tok_first,
			       enum action_where where)
{
	struct trailer_item *arg_tok;
	struct trailer_item *next_arg;

	int tok_alnum_len = alnum_len(infile_tok->token, strlen(infile_tok->token));
	for (arg_tok = *arg_tok_first; arg_tok; arg_tok = next_arg) {
		next_arg = arg_tok->next;
		if (same_token(infile_tok, arg_tok, tok_alnum_len) &&
		    arg_tok->conf->where == where) {
			/* Remove arg_tok from list */
			remove_from_list(arg_tok, arg_tok_first);
			/* Apply arg */
			apply_arg_if_exist(infile_tok, arg_tok, tok_alnum_len);
			/*
			 * If arg has been added to infile,
			 * then we need to process it too now.
			 */
			if ((where == WHERE_AFTER ? infile_tok->next : infile_tok->previous) == arg_tok)
				infile_tok = arg_tok;
		}
	}
}

static void update_last(struct trailer_item **last)
{
	if (*last)
		while((*last)->next != NULL)
			*last = (*last)->next;
}

static void update_first(struct trailer_item **first)
{
	if (*first)
		while((*first)->previous != NULL)
			*first = (*first)->previous;
}

static void apply_arg_if_missing(struct trailer_item **infile_tok_first,
				 struct trailer_item **infile_tok_last,
				 struct trailer_item *arg_tok)
{
	struct trailer_item **infile_tok;
	enum action_where where;

	switch(arg_tok->conf->if_missing) {
	case MISSING_DO_NOTHING:
		free(arg_tok);
		break;
	case MISSING_ADD:
		where = arg_tok->conf->where;
		infile_tok = (where == WHERE_AFTER) ? infile_tok_last : infile_tok_first;
		if (*infile_tok) {
			add_arg_to_infile(*infile_tok, arg_tok);
			*infile_tok = arg_tok;
		} else {
			*infile_tok_first = arg_tok;
			*infile_tok_last = arg_tok;
		}
		break;
	}
}

static void process_trailers_lists(struct trailer_item **infile_tok_first,
				   struct trailer_item **infile_tok_last,
				   struct trailer_item **arg_tok_first)
{
	struct trailer_item *infile_tok;
	struct trailer_item *arg_tok;

	if (!*arg_tok_first)
		return;

	/* Process infile from end to start */
	for (infile_tok = *infile_tok_last; infile_tok; infile_tok = infile_tok->previous) {
		process_infile_tok(infile_tok, arg_tok_first, WHERE_AFTER);
	}

	update_last(infile_tok_last);

	if (!*arg_tok_first)
		return;

	/* Process infile from start to end */
	for (infile_tok = *infile_tok_first; infile_tok; infile_tok = infile_tok->next) {
		process_infile_tok(infile_tok, arg_tok_first, WHERE_BEFORE);
	}

	update_first(infile_tok_first);

	/* Process args left */
	while (*arg_tok_first) {
		arg_tok = remove_first(arg_tok_first);
		apply_arg_if_missing(infile_tok_first, infile_tok_last, arg_tok);
	}
}

static int set_where(struct conf_info *item, const char *value)
{
	if (!strcasecmp("after", value)) {
		item->where = WHERE_AFTER;
	} else if (!strcasecmp("before", value)) {
		item->where = WHERE_BEFORE;
	} else
		return 1;
	return 0;
}

static int set_if_exist(struct conf_info *item, const char *value)
{
	if (!strcasecmp("addIfDifferent", value)) {
		item->if_exist = EXIST_ADD_IF_DIFFERENT;
	} else if (!strcasecmp("addIfDifferentNeighbor", value)) {
		item->if_exist = EXIST_ADD_IF_DIFFERENT_NEIGHBOR;
	} else if (!strcasecmp("add", value)) {
		item->if_exist = EXIST_ADD;
	} else if (!strcasecmp("overwrite", value)) {
		item->if_exist = EXIST_OVERWRITE;
	} else if (!strcasecmp("doNothing", value)) {
		item->if_exist = EXIST_DO_NOTHING;
	} else
		return 1;
	return 0;
}

static int set_if_missing(struct conf_info *item, const char *value)
{
	if (!strcasecmp("doNothing", value)) {
		item->if_missing = MISSING_DO_NOTHING;
	} else if (!strcasecmp("add", value)) {
		item->if_missing = MISSING_ADD;
	} else
		return 1;
	return 0;
}

enum trailer_info_type { TRAILER_VALUE, TRAILER_COMMAND, TRAILER_WHERE,
			 TRAILER_IF_EXIST, TRAILER_IF_MISSING };

static int set_name_and_type(const char *conf_key, const char *suffix,
			     enum trailer_info_type type,
			     char **pname, enum trailer_info_type *ptype)
{
	int ret = ends_with(conf_key, suffix);
	if (ret) {
		*pname = xstrndup(conf_key, strlen(conf_key) - strlen(suffix));
		*ptype = type;
	}
	return ret;
}

static struct trailer_item *get_conf_item(char *name)
{
	struct trailer_item *item;
	struct trailer_item *previous;

	/* Look up item with same name */
	for (previous = NULL, item = first_conf_item;
	     item;
	     previous = item, item = item->next)
	{
		if (!strcasecmp(item->conf->name, name))
			return item;
	}

	/* Item does not already exists, create it */
	item = xcalloc(sizeof(struct trailer_item), 1);
	item->conf = xcalloc(sizeof(struct conf_info), 1);
	item->conf->name = xstrdup(name);

	if (!previous) {
		first_conf_item = item;
	} else {
		previous->next = item;
		item->previous = previous;
	}

	return item;
}

static int git_trailer_config(const char *conf_key, const char *value, void *cb)
{
	if (starts_with(conf_key, "trailer.")) {
		const char *orig_conf_key = conf_key;
		struct trailer_item *item;
		struct conf_info *conf;
		char *name;
		enum trailer_info_type type;

		conf_key += 8;
		if (!set_name_and_type(conf_key, ".key", TRAILER_VALUE, &name, &type) &&
		    !set_name_and_type(conf_key, ".command", TRAILER_COMMAND, &name, &type) &&
		    !set_name_and_type(conf_key, ".where", TRAILER_WHERE, &name, &type) &&
		    !set_name_and_type(conf_key, ".ifexist", TRAILER_IF_EXIST, &name, &type) &&
		    !set_name_and_type(conf_key, ".ifmissing", TRAILER_IF_MISSING, &name, &type))
			return 0;

		item = get_conf_item(name);
		conf = item->conf;

		if (type == TRAILER_VALUE) {
			if (conf->key)
				warning(_("more than one %s"), orig_conf_key);
			conf->key = xstrdup(value);
		} else if (type == TRAILER_COMMAND) {
			if (conf->command)
				warning(_("more than one %s"), orig_conf_key);
			conf->command = xstrdup(value);
			conf->command_uses_arg = !!strstr(conf->command, TRAILER_ARG_STRING);
		} else if (type == TRAILER_WHERE) {
			if (set_where(conf, value))
				warning(_("unknown value '%s' for key '%s'"), value, orig_conf_key);
		} else if (type == TRAILER_IF_EXIST) {
			if (set_if_exist(conf, value))
				warning(_("unknown value '%s' for key '%s'"), value, orig_conf_key);
		} else if (type == TRAILER_IF_MISSING) {
			if (set_if_missing(conf, value))
				warning(_("unknown value '%s' for key '%s'"), value, orig_conf_key);
		} else {
			die("internal bug in trailer.c");
		}
	}
	return 0;
}

static void parse_trailer(struct strbuf *tok, struct strbuf *val, const char *trailer)
{
	char *end = strchr(trailer, '=');
	if (!end)
		end = strchr(trailer, ':');
	if (end) {
		strbuf_add(tok, trailer, end - trailer);
		strbuf_trim(tok);
		strbuf_addstr(val, end + 1);
		strbuf_trim(val);
	} else {
		strbuf_addstr(tok, trailer);
		strbuf_trim(tok);
	}
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
	const char *result = "";

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

	if (read_from_command(&cp, &buf))
		strbuf_release(&buf);
	else
		result = strbuf_detach(&buf, NULL);

	strbuf_release(&cmd);
	return result;
}

static struct trailer_item *new_trailer_item(struct trailer_item *conf_item,
					     const char* tok, const char* val)
{
	struct trailer_item *new = xcalloc(sizeof(struct trailer_item), 1);
	new->value = val;

	if (conf_item) {
		new->conf = conf_item->conf;
		new->token = xstrdup(conf_item->conf->key);
		if (conf_item->conf->command_uses_arg || !val)
			new->value = apply_command(conf_item->conf->command, val);
	} else {
		new->conf = xcalloc(sizeof(struct conf_info), 1);
		new->token = tok;
	}

	return new;
}

static struct trailer_item *create_trailer_item(const char *string)
{
	struct strbuf tok = STRBUF_INIT;
	struct strbuf val = STRBUF_INIT;
	int tok_alnum_len;
	struct trailer_item *item;

	parse_trailer(&tok, &val, string);

	tok_alnum_len = alnum_len(tok.buf, tok.len);

	/* Lookup if the token matches something in the config */
	for (item = first_conf_item; item; item = item->next)
	{
		if (!strncasecmp(tok.buf, item->conf->key, tok_alnum_len) ||
		    !strncasecmp(tok.buf, item->conf->name, tok_alnum_len)) {
			strbuf_release(&tok);
			return new_trailer_item(item, NULL, strbuf_detach(&val, NULL));
		}
	}

	return new_trailer_item(NULL, strbuf_detach(&tok, NULL), strbuf_detach(&val, NULL));;
}

static void add_trailer_item(struct trailer_item **first,
			     struct trailer_item **last,
			     struct trailer_item *new)
{
	if (!*last) {
		*first = new;
		*last = new;
	} else {
		(*last)->next = new;
		new->previous = *last;
		*last = new;
	}
}

static struct trailer_item *process_command_line_args(int argc, const char **argv)
{
	int i;
	struct trailer_item *arg_tok_first = NULL;
	struct trailer_item *arg_tok_last = NULL;
	struct trailer_item *item;

	for (i = 0; i < argc; i++) {
		struct trailer_item *new = create_trailer_item(argv[i]);
		add_trailer_item(&arg_tok_first, &arg_tok_last, new);
	}

	/* Add conf commands that don't use $ARG */
	for (item = first_conf_item; item; item = item->next)
	{
		if (item->conf->command && !item->conf->command_uses_arg)
		{
			struct trailer_item *new = new_trailer_item(item, NULL, NULL);
			add_trailer_item(&arg_tok_first, &arg_tok_last, new);
		}
	}

	return arg_tok_first;
}

static struct strbuf **read_input_file(const char *infile)
{
	struct strbuf sb = STRBUF_INIT;

	if (infile) {
		if (strbuf_read_file(&sb, infile, 0) < 0)
			die_errno(_("could not read input file '%s'"), infile);
	} else {
		if (strbuf_read(&sb, fileno(stdin), 0) < 0)
			die_errno(_("could not read from stdin"));
	}

	return strbuf_split(&sb, '\n');
}

/*
 * Return the the (0 based) index of the first trailer line
 * or the line count if there are no trailers.
 */
static int find_trailer_start(struct strbuf **lines)
{
	int count, start, empty = 1;

	/* Get the line count */
	for (count = 0; lines[count]; count++);

	/*
	 * Get the start of the trailers by looking starting from the end
	 * for a line with only spaces before lines with one ':'.
	 */
	for (start = count - 1; start >= 0; start--) {
		if (strbuf_isspace(lines[start])) {
			if (empty)
				continue;
			return start + 1;
		}
		if (strchr(lines[start]->buf, ':')) {
			if (empty)
				empty = 0;
			continue;
		}
		return count;
	}

	return empty ? count : start + 1;
}

static void process_input_file(const char *infile,
			       struct trailer_item **infile_tok_first,
			       struct trailer_item **infile_tok_last)
{
	struct strbuf **lines = read_input_file(infile);
	int start = find_trailer_start(lines);
	int i;

	/* Print non trailer lines as is */
	for (i = 0; lines[i] && i < start; i++) {
		printf("%s", lines[i]->buf);
	}

	/* Parse trailer lines */
	for (i = start; lines[i]; i++) {
		struct trailer_item *new = create_trailer_item(lines[i]->buf);
		add_trailer_item(infile_tok_first, infile_tok_last, new);
	}
}

void process_trailers(const char *infile, int trim_empty, int argc, const char **argv)
{
	struct trailer_item *infile_tok_first = NULL;
	struct trailer_item *infile_tok_last = NULL;
	struct trailer_item *arg_tok_first;

	git_config(git_trailer_config, NULL);

	/* Print the non trailer part of infile (or stdin if infile is NULL) */
	process_input_file(infile, &infile_tok_first, &infile_tok_last);

	arg_tok_first = process_command_line_args(argc, argv);

	process_trailers_lists(&infile_tok_first, &infile_tok_last, &arg_tok_first);

	print_all(infile_tok_first, trim_empty);
}
