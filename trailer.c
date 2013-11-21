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
	
}

void apply_arg_if_exist(struct string_list_item *infile_item,
			struct string_list_item *arg_item,
			enum action_where where,
			struct string_list *infile_tok_list,
			int cur_index)
{
	struct tok_info *infile_info = infile_item->util;
	struct tok_info *arg_info = arg_item->util;

	infile_info->conf = arg_info->conf;

	if (arg_info->applied)
		return;
	if (arg_info->conf->where != where)
		return;

	switch(arg_info->conf->if_exist) {
	case EXIST_DO_NOTHING:
		break;
	case EXIST_OVERWRITE:
		free(infile_info->value);
		infile_info->value = xstrdup(arg_info->value);
		arg_info->applied = 1;
		break;
	case EXIST_ADD:
		add_arg_to_infile(infile_tok_list, cur_index + 1, arg_item);
		break;
	case EXIST_ADD_IF_DIFFERENT:
		/* TODO: Check if different */
		add_arg_to_infile(infile_tok_list, cur_index + 1, arg_item);
		break;
	case EXIST_ADD_IF_DIFFERENT_NEIGHBOR:
		/* TODO: Check if different neighbor */
		add_arg_to_infile(infile_tok_list, cur_index + 1, arg_item);
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
				apply_arg_if_exist(infile_item, arg_item, AFTER, infile_tok_list, i);
			}
		}
	}

	/* Process infile from start to end */
	for (i = 0; i < infile_tok_list.nr; i++) {
	}
}
		      
