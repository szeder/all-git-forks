#ifndef SHORTLOG_H
#define SHORTLOG_H

#include "string-list.h"

#define DEFAULT_WRAPLEN 76
#define DEFAULT_INDENT1 6
#define DEFAULT_INDENT2 9

struct shortlog {
	struct string_list list;
	int summary;
	int wrap_lines;
	int sort_by_number;
	int wrap;
	int in1;
	int in2;
	int user_format;
	int abbrev;

	char *common_repo_prefix;
	int email;
	struct string_list mailmap;
};

struct commit;

void shortlog_init(struct shortlog *log);

void shortlog_add_commit(struct shortlog *log, struct commit *commit);

void shortlog_insert_one_record(struct shortlog *log, const char *author, const char *oneline);

void shortlog_output(struct shortlog *log);

#endif
