#ifndef META_H
#define META_H

#include <stddef.h>
#include <time.h>
#include "hash.h"
#include "git-compat-util.h"
#include "strbuf.h"
#include "string-list.h"
#include "vcs-cvs/cvs-types.h"
#include "vcs-cvs/aggregator.h"

unsigned int hash_path(const char *path);

void set_ref_prefix_remote(const char *remote_name);
const char *get_meta_ref_prefix();
const char *get_ref_prefix();
const char *get_private_ref_prefix();

/*
 * return -1 on error
 * revision_meta_hash == NULL if metadata was not loaded
 */
int load_revision_meta(unsigned char *sha1, const char *notes_ref, time_t *timestamp, struct hash_table **revision_meta_hash);
int save_revision_meta(unsigned char *sha1, const char *notes_ref, const char *msg, struct hash_table *revision_meta_hash);
int has_revision_meta(unsigned char *sha1, const char *notes_ref);

char *read_note_of(unsigned char sha1[20], const char *notes_ref, unsigned long *size);
//char *parse_meta_line(char *buf, unsigned long len, char **first, char **second, struct string_list *attr_lst, char *p);
char *parse_meta_line(char *buf, unsigned long len, char **first, char **second, char *p);
/*
revision=1.24.3.43,isdead=y,ispushed=y:path
 */
void format_meta_line(struct strbuf *line, struct cvs_revision *meta);

#endif
