#include "cache.h"
/*
 * Copyright (c) 2013, 2014 Christian Couder <chriscool@tuxfamily.org>
 */

enum action_where { WHERE_END, WHERE_AFTER, WHERE_BEFORE, WHERE_START };
enum action_if_exists { EXISTS_ADD_IF_DIFFERENT_NEIGHBOR, EXISTS_ADD_IF_DIFFERENT,
			EXISTS_ADD, EXISTS_REPLACE, EXISTS_DO_NOTHING };
enum action_if_missing { MISSING_ADD, MISSING_DO_NOTHING };

struct conf_info {
	char *name;
	char *key;
	char *command;
	enum action_where where;
	enum action_if_exists if_exists;
	enum action_if_missing if_missing;
};

static struct conf_info default_conf_info;

struct trailer_item {
	struct trailer_item *previous;
	struct trailer_item *next;
	const char *token;
	const char *value;
	struct conf_info conf;
};

static struct trailer_item *first_conf_item;

static char *separators = ":";

static int after_or_end(enum action_where where)
{
	return (where == WHERE_AFTER) || (where == WHERE_END);
}

/* Get the length of buf from its beginning until its last alphanumeric character */
static size_t alnum_len(const char *buf, size_t len)
{
	while (len > 0 && !isalnum(buf[len - 1]))
		len--;
	return len;
}

static int same_token(struct trailer_item *a, struct trailer_item *b)
{
	size_t a_alnum_len = alnum_len(a->token, strlen(a->token));
	size_t b_alnum_len = alnum_len(b->token, strlen(b->token));
	size_t min_alnum_len = (a_alnum_len > b_alnum_len) ? b_alnum_len : a_alnum_len;

	return !strncasecmp(a->token, b->token, min_alnum_len);
}

static int same_value(struct trailer_item *a, struct trailer_item *b)
{
	return !strcasecmp(a->value, b->value);
}

static int same_trailer(struct trailer_item *a, struct trailer_item *b)
{
	return same_token(a, b) && same_value(a, b);
}
