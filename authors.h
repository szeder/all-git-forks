#ifndef AUTHORS_H
#define AUTHORS_H

#include "string-list.h"

struct authors_split {
	const char *begin;
	const char *end;
};
/*
 * Signals an success with 0
 */
extern int split_authors_line(struct authors_split *, const char *, int);

extern const char *git_authors_info(void);
extern const char *git_authors_first_info(const char *);

extern void read_authors_map_file(struct string_list *);

extern char *lookup_author(struct string_list *, const char *);
extern const char *expand_authors(struct string_list *, const char *);

extern const char *authors_split_to_email_froms(const struct authors_split *);

extern int has_multiple_authors(const char *);

extern const char *fmt_authors(const char *, const char *);

#endif /* AUTHORS_H */
