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

struct tok_info {
	struct conf_info *conf;
	const char *value;
	int applied;
}

/* Get the length of buf from its beginning until its last alphanumeric character */
static size_t alnum_len(const char *buf, size_t len) {
	while (--len >= 0 && !isalnum(buf[len]));
	return len + 1;
}

void add_arg_to_infile(struct string_list *infile_tok_list,
		       int index,
		       struct string_list_item *arg_item)
{
	struct string_list_item *new_tok = string_list_insert_at_index(infile_tok_list,
								       cur_index + 1,
								       arg_item->string);
	new_tok->util = arg_item->util;
}

static int check_if_previous_different(struct string_list_item *infile_item,
				       struct string_list_item *arg_item,
				       struct string_list *infile_tok_list,
				       int cur_index, int min_index, int alnum_len)
{
	int i;
	if (min_index < 0)
		min_index = 0;
	for (i = cur_index - 1; i >= min_index; i--) {
		if (!strncasecmp(infile_item->string, arg_item->string, alnum_len)) {
			struct tok_info *infile_info = infile_item->util;
			struct tok_info *arg_info = arg_item->util;
			if (!strcasecmp(infile_info->value, arg_info->value))
				return 0;
		}
	}
	return 1;
}

static int check_if_next_different(struct string_list_item *infile_item,
				   struct string_list_item *arg_item,
				   struct string_list *infile_tok_list,
				   int cur_index, int max_index, int alnum_len)
{
	int i;
	if (max_index > infile_tok_list.nr)
		max_index = infile_tok_list.nr;
	for (i = cur_index + 1; i <= max_index; i++) {
		if (!strncasecmp(infile_item->string, arg_item->string, alnum_len)) {
			struct tok_info *infile_info = infile_item->util;
			struct tok_info *arg_info = arg_item->util;
			if (!strcasecmp(infile_info->value, arg_info->value))
				return 0;
		}
	}
	return 1;
}

void apply_arg_if_exist(struct string_list_item *infile_item,
			struct string_list_item *arg_item,
			enum action_where where,
			struct string_list *infile_tok_list,
			int cur_index, int alnum_len)
{
	struct tok_info *infile_info = infile_item->util;
	struct tok_info *arg_info = arg_item->util;
	int add_index, different;

	infile_info->conf = arg_info->conf;

	if (arg_info->applied)
		return;
	if (arg_info->conf->where != where)
		return;

	add_index = (where == AFTER) ? cur_index + 1 : cur_index - 1;

	switch(arg_info->conf->if_exist) {
	case EXIST_DO_NOTHING:
		break;
	case EXIST_OVERWRITE:
		free(infile_info->value);
		infile_info->value = xstrdup(arg_info->value);
		break;
	case EXIST_ADD:
		add_arg_to_infile(infile_tok_list, add_index, arg_item);
		break;
	case EXIST_ADD_IF_DIFFERENT:
		if (where == AFTER)
			different = check_if_previous_different(infile_item, arg_item,
								infile_tok_list,
								cur_index, 0, alnum_len);
		else
			different = check_if_next_different(infile_item, arg_item,
							    infile_tok_list,
							    cur_index, infile_tok_list.nr, alnum_len);
		if (different)
			add_arg_to_infile(infile_tok_list, add_index, arg_item);
		break;
	case EXIST_ADD_IF_DIFFERENT_NEIGHBOR:
		if (where == AFTER)
			different = check_if_previous_different(infile_item, arg_item,
								infile_tok_list,
								cur_index, cur_index - 1, alnum_len);
		else
			different = check_if_next_different(infile_item, arg_item,
							    infile_tok_list,
							    cur_index, cur_index + 1, alnum_len);
		if (different)
			add_arg_to_infile(infile_tok_list, add_index, arg_item);
		break;
	}
	arg_info->applied = 1;
}

void process_trailers(struct string_list *infile_tok_list,
		      struct string_list *arg_tok_list)
{
	int i, j;

	/* Process infile from end to start */
	for (i = infile_tok_list.nr - 1; i >= 0; i--) {
		struct string_list_item *infile_item = infile_tok_list.items + i;
		int tok_alnum_len = alnum_len(infile_item->string, strlen(infile_item->string));
		for (j = 0; j < arg_tok_list.nr; j++) {
			struct string_list_item *arg_item = arg_tok_list.items + j;
			if (!strncasecmp(infile_item->string, arg_item->string, tok_alnum_len)) {
				apply_arg_if_exist(infile_item, arg_item, AFTER,
						   infile_tok_list, i, tok_alnum_len);
			}
		}
	}

	/* Process infile from start to end */
	for (i = 0; i < infile_tok_list.nr; i++) {
	}
}
		      
