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

#endif /* IDENT_SCRIPT_H */
