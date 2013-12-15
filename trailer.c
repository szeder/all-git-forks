#include "cache.h"

enum action_where { AFTER, BEFORE };
enum action_if_exist { EXIST_ADD_IF_DIFFERENT, EXIST_ADD_IF_DIFFERENT_NEIGHBOR,
		       EXIST_ADD, EXIST_OVERWRITE, EXIST_DO_NOTHING };
enum action_if_missing { MISSING_ADD, MISSING_DO_NOTHING };

struct conf_info {
	char *name;
	char *key;
	char *command;
	enum action_where where;
	enum action_if_exist if_exist;
	enum action_if_missing if_missing;
};

struct trailer_item {
	struct trailer_item *previous;
	struct trailer_item *next;
	const char *token;
	const char *value;
	struct conf_info *conf;
};

struct trailer_item *first_conf_item;

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

static void add_arg_to_infile(struct trailer_item *infile_tok,
			      struct trailer_item *arg_tok)
{
	if (arg_tok->conf->where == AFTER) {
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
		/*
		 * if we want to add a trailer after another one,
		 * we have to check those before this one
		 */
		infile_tok = (where == AFTER) ? infile_tok->previous : infile_tok->next;
		if (!infile_tok)
			return 1;
		if (same_trailer(infile_tok, arg_tok, alnum_len))
			return 0;
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

static struct trailer_item *remove_from_list(struct trailer_item *arg_tok,
					     struct trailer_item *arg_tok_first)
{
	if (arg_tok->next)
		arg_tok->next->previous = arg_tok->previous;
	if (arg_tok->previous)
		arg_tok->previous->next = arg_tok->next;
	else
		arg_tok_first = arg_tok->next;

	return arg_tok_first;
}

static struct trailer_item *process_inline_tok(struct trailer_item *infile_tok,
					       struct trailer_item *arg_tok_first,
					       enum action_where where)
{
	struct trailer_item *arg_tok;
	struct trailer_item *next_arg;

	int tok_alnum_len = alnum_len(infile_tok->token, strlen(infile_tok->token));
	for (arg_tok = arg_tok_first; arg_tok; arg_tok = next_arg) {
		next_arg = arg_tok->next;
		if (same_token(infile_tok, arg_tok, tok_alnum_len) &&
		    arg_tok->conf->where == where) {
			/* Remove arg_tok from list */
			arg_tok_first = remove_from_list(arg_tok, arg_tok_first);
			/* Apply arg */
			apply_arg_if_exist(infile_tok, arg_tok, tok_alnum_len);
			/*
			 * If arg has been added to infile,
			 * then we need to process it too now.
			 */
			if ((where == AFTER ? infile_tok->next : infile_tok->previous) == arg_tok)
				infile_tok = arg_tok;
		}
	}
	return arg_tok_first;
}

static struct trailer_item *update_last(struct trailer_item *last)
{
	while(last->next != NULL)
		last = last->next;
	return last;
}

static struct trailer_item *update_first(struct trailer_item *first)
{
	while(first->previous != NULL)
		first = first->previous;
	return first;
}

static void apply_arg_if_missing(struct trailer_item *infile_tok_first,
				 struct trailer_item *infile_tok_last,
				 struct trailer_item *arg_tok)
{
	struct trailer_item *infile_tok;
	enum action_where where;

	switch(arg_tok->conf->if_missing) {
	case MISSING_DO_NOTHING:
		free(arg_tok);
		break;
	case MISSING_ADD:
		where = arg_tok->conf->where;
		infile_tok = (where == AFTER) ? infile_tok_last : infile_tok_first;
		add_arg_to_infile(infile_tok, arg_tok);
		break;
	}
}

static void process_trailers(struct trailer_item *infile_tok_first,
			     struct trailer_item *infile_tok_last,
			     struct trailer_item *arg_tok_first)
{
	struct trailer_item *infile_tok;
	struct trailer_item *arg_tok;
	struct trailer_item *next_arg;

	if (!arg_tok_first)
		return;

	/* Process infile from end to start */
	for (infile_tok = infile_tok_last; infile_tok; infile_tok = infile_tok->previous) {
		arg_tok_first = process_inline_tok(infile_tok, arg_tok_first, AFTER);
	}

	infile_tok_last = update_last(infile_tok_last);

	if (!arg_tok_first)
		return;

	/* Process infile from start to end */
	for (infile_tok = infile_tok_first; infile_tok; infile_tok = infile_tok->next) {
		arg_tok_first = process_inline_tok(infile_tok, arg_tok_first, BEFORE);
	}

	infile_tok_first = update_first(infile_tok_first);

	if (!arg_tok_first)
		return;

	/* Process args left */
	for (arg_tok = arg_tok_first; arg_tok; arg_tok = next_arg) {
		next_arg = arg_tok->next;
		arg_tok_first = remove_from_list(arg_tok, arg_tok_first);
		apply_arg_if_missing(infile_tok_first, infile_tok_last, arg_tok);
	}
}
		      
static int set_where(struct conf_info *item, const char *value)
{
	if (!strcasecmp("after", value)) {
		item->where = AFTER;
	} else if (!strcasecmp("before", value)) {
		item->where = BEFORE;
	} else
		return 1;
	return 0;
}

static int set_if_exist(struct conf_info *item, const char *value)
{
	if (!strcasecmp("add_if_different", value)) {
		item->if_exist = EXIST_ADD_IF_DIFFERENT;
	} else if (!strcasecmp("add_if_different_neighbor", value)) {
		item->if_exist = EXIST_ADD_IF_DIFFERENT_NEIGHBOR;
	} else if (!strcasecmp("add", value)) {
		item->if_exist = EXIST_ADD;
	} else if (!strcasecmp("overwrite", value)) {
		item->if_exist = EXIST_OVERWRITE;
	} else if (!strcasecmp("do_nothing", value)) {
		item->if_exist = EXIST_DO_NOTHING;
	} else
		return 1;
	return 0;
}

static int set_if_missing(struct conf_info *item, const char *value)
{
	if (!strcasecmp("do_nothing", value)) {
		item->if_missing = MISSING_DO_NOTHING;
	} else if (!strcasecmp("add", value)) {
		item->if_missing = MISSING_ADD;
	} else
		return 1;
	return 0;
}

enum trailer_info_type { VALUE, COMMAND, WHERE, IF_EXIST, IF_MISSING };

static int set_name_and_type(const char *conf_key, const char *suffix,
			     enum trailer_info_type type,
			     char **pname, enum trailer_info_type *ptype)
{
	int ret = !suffixcmp(conf_key, suffix);
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

	if (!previous)
		first_conf_item = item;
	else {
		previous->next = item;
		item->previous = previous;
	}

	return item;
}

static int git_trailer_config(const char *conf_key, const char *value, void *cb)
{
	if (!prefixcmp(conf_key, "trailer.")) {
		const char *orig_conf_key = conf_key;
		struct trailer_item *item;
		struct conf_info *conf;
		char *name;
		enum trailer_info_type type;

		conf_key += 8;
		if (!set_name_and_type(conf_key, ".key", VALUE, &name, &type) &&
		    !set_name_and_type(conf_key, ".command", COMMAND, &name, &type) &&
		    !set_name_and_type(conf_key, ".where", WHERE, &name, &type) &&
		    !set_name_and_type(conf_key, ".if_exist", IF_EXIST, &name, &type) &&
		    !set_name_and_type(conf_key, ".if_missing", IF_MISSING, &name, &type))
			return 0;

		item = get_conf_item(name);
		conf = item->conf;

		if (type == VALUE) {
			if (conf->key)
				warning(_("more than one %s"), orig_conf_key);
			conf->key = xstrdup(value);
		} else if (type == COMMAND) {
			if (conf->command)
				warning(_("more than one %s"), orig_conf_key);
			conf->command = xstrdup(value);
		} else if (type == WHERE) {
			if (set_where(conf, value))
				warning(_("unknow value '%s' for key '%s'"), value, orig_conf_key);
		} else if (type == IF_EXIST) {
			if (set_if_exist(conf, value))
				warning(_("unknow value '%s' for key '%s'"), value, orig_conf_key);
		} else if (type == IF_MISSING) {
			if (set_if_missing(conf, value))
				warning(_("unknow value '%s' for key '%s'"), value, orig_conf_key);
		} else {
			die("internal bug in trailer.c");
		}
	}
	return 0;
}

static void parse_arg_trailer(struct strbuf *tok, struct strbuf *val, const char *trailer)
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

static void apply_config_value(struct strbuf *tok, struct strbuf *val, struct trailer_info *info)
{
	if (info->value) {
		strbuf_reset(tok);
		strbuf_addstr(tok, info->value);
	}
}

static struct trailer_item *apply_config_to_arg(const char *arg)
{
	struct strbuf tok = STRBUF_INIT;
	struct strbuf val = STRBUF_INIT;

	parse_trailer(&tok, &val, arg);

	int tok_alnum_len = alnum_len(tok.buf, tok.len);

	/* Lookup if the arg matches something in the config */
	struct trailer_item *item;
	for (item = first_conf_item; item; item = item->next)
	{
		if (!strncasecmp(tok->buf, item->conf->key, tok_alnum_len) ||
		    !strncasecmp(tok->buf, info->conf->name, tok_alnum_len)) {
			struct trailer_item *arg_tok = xcalloc(sizeof(struct trailer_item), 1);
			arg_tok->conf = item->conf;
			arg_tok->token = xstrdup(item->conf->key);
			arg_tok->value = strbuf_detach(&val, NULL);
			strbuf_release(&tok);
			return arg_tok;
		}
	}

	struct trailer_item *arg_tok = xcalloc(sizeof(struct trailer_item), 1);
	arg_tok->token = strbuf_detach(&tok, NULL);
	arg_tok->value = strbuf_detach(&val, NULL);

	return arg_tok;
}

void process_command_line_args(int argc, const char **argv)
{
	int i;
	struct trailer_item *arg_tok_first = NULL;
	struct trailer_item *arg_tok_last = NULL;

	for (i = 0; i < argc; i++) {
		struct trailer_item *arg_tok_new = apply_config_to_arg(argv[i]);
		if (!arg_tok_last) {
			arg_tok_first = arg_tok_new;
			arg_tok_last = arg_tok_new;
		} else {
			arg_tok_last->next = arg_tok_new;
			arg_tok_new->previous = arg_tok_last;
			arg_tok_last = arg_tok_new;
		}
	}
}
