#ifndef IDENT_SCRIPT_H
#define IDENT_SCRIPT_H

struct ident_script {
	char *name;
	char *email;
	char *date;
};

#define IDENT_SCRIPT_INIT { NULL, NULL, NULL }

void ident_script_init(struct ident_script *);

void ident_script_release(struct ident_script *);

/**
 * Reads and parses the state directory's "author-script" file, and sets
 * state->author_name, state->author_email and state->author_date accordingly.
 * Returns 0 on success, -1 if the file could not be parsed.
 *
 * The author script is of the format:
 *
 *	GIT_{VAR}_NAME='$author_name'
 *	GIT_{VAR}_EMAIL='$author_email'
 *	GIT_{VAR}_DATE='$author_date'
 *
 * where $author_name, $author_email and $author_date are quoted. We are strict
 * with our parsing, as the file was meant to be eval'd in the old git-am.sh
 * script, and thus if the file differs from what this function expects, it is
 * better to bail out than to do something that the user does not expect.
 */
int read_ident_script(struct ident_script *, const char *filename, const char *var);

/**
 * Saves state->author_name, state->author_email and state->author_date in the
 * state directory's "author-script" file.
 */
void write_ident_script(const struct ident_script *, const char *filename, const char *var);

int ident_script_from_line(struct ident_script *, const char *, size_t);

#endif
