#include "cache.h"

enum action_where { AFTER, MIDDLE, BEFORE };
enum action_if_exist { EXIST_ADD_IF_DIFFERENT, EXIST_ADD_IF_DIFFERENT_NEIGHBOR,
		       EXIST_ADD, EXIST_OVERWRITE, EXIST_DO_NOTHING };
enum action_if_missing { MISSING_DO_NOTHING, MISSING_ADD };

struct conf_info {
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
	int applied;
};

/* Get the length of buf from its beginning until its last alphanumeric character */
static size_t alnum_len(const char *buf, size_t len) {
	while (--len >= 0 && !isalnum(buf[len]));
	return len + 1;
}

void add_arg_to_infile(struct trailer_item *infile_tok,
		       struct trailer_item *arg_tok,
		       enum action_where where)
{
	if (where == AFTER) {
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

static int check_if_different(struct trailer_item *infile_tok,
			      struct trailer_item *arg_tok,
			      int alnum_len, int check_all,
			      enum action_where where)
{
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

void apply_arg_if_exist(struct trailer_item *infile_tok,
			struct trailer_item *arg_tok,
			int alnum_len,
			enum action_where where)
{
	switch(arg_tok->conf->if_exist) {
	case EXIST_DO_NOTHING:
		free(arg_tok);
		break;
	case EXIST_OVERWRITE:
		free(infile_tok->value);
		infile_tok->value = xstrdup(arg_tok->value);
		free(arg_tok);
		break;
	case EXIST_ADD:
		add_arg_to_infile(infile_tok, arg_tok, where);
		break;
	case EXIST_ADD_IF_DIFFERENT:
		if (check_if_different(infile_tok, arg_tok, alnum_len, 1, where))
			add_arg_to_infile(infile_tok, arg_tok, where);
		else
			free(arg_tok);
		break;
	case EXIST_ADD_IF_DIFFERENT_NEIGHBOR:
		if (check_if_different(infile_tok, arg_tok, alnum_len, 0, where))
			add_arg_to_infile(infile_tok, arg_tok, where);
		else
			free(arg_tok);
		break;
	}
}

struct trailer_item *process_inline_tok(struct trailer_item *infile_tok,
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
			if (next_arg)
				next_arg->previous = arg_tok->previous;
			if (arg_tok->previous)
				arg_tok->previous->next = next_arg;
			else
				arg_tok_first = next_arg;
			/* Apply arg */
			apply_arg_if_exist(infile_tok, arg_tok, tok_alnum_len, where);
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

void process_trailers(struct trailer_item *infile_tok_first,
		      struct trailer_item *infile_tok_last,
		      struct trailer_item *arg_tok_first)
{
	struct trailer_item *infile_tok;

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

	int tok_alnum_len = alnum_len(infile_tok->token, strlen(infile_tok->token));
	for (arg_tok = arg_tok_first; arg_tok; arg_tok = next_arg) {
		next_arg = arg_tok->next;
		if (arg_tok->conf->where == AFTER)
		{

		} else { /* arg_tok->conf->where == BEFORE */

		}
}
		      
