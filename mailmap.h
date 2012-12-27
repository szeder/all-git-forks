#ifndef MAILMAP_H
#define MAILMAP_H

int read_mailmap(struct string_list *map, char **repo_abbrev);
void clear_mailmap(struct string_list *map);

int map_user(struct string_list *map, struct strbuf *name, struct strbuf *mail);
#endif
