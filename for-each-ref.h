#ifndef FOR_EACH_REF_H
#define FOR_EACH_REF_H

enum quote_style {
	QUOTE_NONE = 0,
	QUOTE_SHELL = 1,
	QUOTE_PERL = 2,
	QUOTE_PYTHON = 4,
	QUOTE_TCL = 8,
};

struct atom_value {
	const char *s;
	unsigned long ul; /* used for sorting when not FIELD_STR */
};

struct refinfo {
	char *refname;
	unsigned char objectname[20];
	int flag;
	const char *symref;
	struct atom_value *value;
};

void show_ref(struct strbuf *sb, struct refinfo *info, const char *format, int quote_style);
void show_refs(struct refinfo **refs, int maxcount, const char *format, int quote_style);

#endif
