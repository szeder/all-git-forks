#include "cache.h"
#include "authors.h"
#include "string.h"
#include "strbuf.h"

/*
 * given an authors line, split the fields
 * to allow the caller to parse it.
 * Signal a success by returning 0.
 */
int split_authors_line(struct authors_split *split, const char *line, int len)
{
	const char *cp;

	memset(split, 0, sizeof(*split));

	split->begin = line;

	for (cp = line + len - 1; *cp != '>'; cp--)
		if (cp == line) return -1;

	split->end = cp + 1;
	return 0;
}

void read_authors_map_line(struct string_list *map, char *buffer)
{
	int len = strlen(buffer);

	if (len && buffer[len - 1] == '\n')
		buffer[--len] = 0;

	string_list_insert(map, xstrdup(buffer));
}

void read_authors_map_file(struct string_list *map)
{
	char buffer[1024];
	FILE *f;
	const char *filename;
	const char *home;

	home = getenv("HOME");
	if (!home)
		die("HOME not set");

	filename = mkpathdup("%s/.git_authors_map", home);

	f = fopen(filename, "r");
	if (!f) {
		if (errno == ENOENT) {
			warning("~/.git_authors_map does not exist");
			return;
		}
		die_errno("unable to open authors map at %s", filename);
	}

	while (fgets(buffer, sizeof(buffer), f) != NULL)
		read_authors_map_line(map, buffer);
	fclose(f);
}

char *lookup_author(struct string_list *map, const char *author_abbr)
{
	struct string_list_item *author_item = NULL;
	struct string_list_item *item;

	for_each_string_list_item(item, map) {
		if (strncmp(item->string, author_abbr, strlen(author_abbr)) == 0 &&
		    strlen(item->string) > strlen(author_abbr) &&
		    *(item->string + strlen(author_abbr)) == ' ') {
			author_item = item;
			break;
		}
	}

	if (!author_item)
		return NULL;

	return xstrdup(author_item->string + strlen(author_abbr) + 1);
}

const char *expand_authors(struct string_list *map, const char *author_shorts)
{
	int i;
	const char *author_start = author_shorts;
	const char *author_end;
	char *author_short, *expanded_author;
	static struct strbuf expanded_authors = STRBUF_INIT;

	strbuf_reset(&expanded_authors);

	for (i = 0; i <= strlen(author_shorts); i++) {
		author_end = author_shorts + i;
		if (*author_end == ' ' || *author_end == '\0') {
			author_short = xstrndup(author_start, author_end - author_start);
			expanded_author = lookup_author(map, author_short);
			if (!expanded_author)
				die("Could not expand author '%s'. Add it to the file ~/.git_authors_map.", author_short);
			else {
				if (expanded_authors.len > 0)
					strbuf_addch(&expanded_authors, ',');
				strbuf_addstr(&expanded_authors, expanded_author);
				free(expanded_author);
			}
			free(author_short);

			author_start = author_end + 1;
		}
	}

	return expanded_authors.buf;
}

const char *git_authors_info(void)
{
	static struct strbuf authors_info = STRBUF_INIT;
	const char *authors_config = NULL;
	const char *date_str = NULL;
	struct string_list authors_map = STRING_LIST_INIT_NODUP;

	if (git_config_get_string_const("authors.current", &authors_config))
		return NULL;

	read_authors_map_file(&authors_map);

	strbuf_reset(&authors_info);
	strbuf_addstr(&authors_info, expand_authors(&authors_map, authors_config));

	strbuf_addch(&authors_info, ' ');
	date_str = getenv("GIT_AUTHOR_DATE");
	if (date_str && date_str[0]) {
		if (parse_date(date_str, &authors_info) < 0)
			die("invalid date format: %s", date_str);
	}
	else
		strbuf_addstr(&authors_info, ident_default_date());

	return authors_info.buf;
}

const char *git_authors_first_info(const char *authors)
{
	static struct strbuf authors_first_info = STRBUF_INIT;
	struct authors_split split;
	const char *cp;

	if (split_authors_line(&split, authors, strlen(authors)) < 0)
		die("invalid authors format: %s", authors);

	for (cp = split.begin; cp < split.end; cp++)
		if (*cp == ',')
			break;
	strbuf_add(&authors_first_info, split.begin, cp - split.begin);
	strbuf_add(&authors_first_info, split.end, strlen(authors));

	return authors_first_info.buf;
}

const char *authors_split_to_email_froms(const struct authors_split *authors)
{
	static struct strbuf email_froms = STRBUF_INIT;
	const char *cp;

	strbuf_reset(&email_froms);

	strbuf_addstr(&email_froms, "From: ");
	for(cp = authors->begin; cp < authors->end; cp++)
		if (*cp == ',')
			strbuf_addstr(&email_froms, "\nFrom: ");
		else
			strbuf_addch(&email_froms, *cp);
	strbuf_addch(&email_froms, '\n');

	return email_froms.buf;
}

int has_multiple_authors(const char *authors)
{
	const char *cp = authors;

	while (*cp != '\0')
		if (*cp++ == ',')
			return 1;
	return 0;
}

const char *fmt_authors(const char *authors, const char *date_str)
{
	static struct strbuf authors_info = STRBUF_INIT;

	strbuf_reset(&authors_info);

	strbuf_addstr(&authors_info, authors);

	strbuf_addch(&authors_info, ' ');
	if (date_str && date_str[0]) {
		if (parse_date(date_str, &authors_info) < 0)
			die("invalid date format: %s", date_str);
	}
	else
		strbuf_addstr(&authors_info, ident_default_date());

	return authors_info.buf;
}
