#ifndef CVS_TYPES_H
#define CVS_TYPES_H

#include <time.h>
/*
 * struct represents a revision of a single file in CVS.
 * cvs_commit field is always initialized and used
 * as author and msg placeholder, it may be changed
 * later during patch aggregation
 */
struct cvs_revision {
	char *path;
	char *revision;
	unsigned int ismeta:1;
	unsigned int isdead:1;

	/*
	 * multiple revisions of a file was merged together during patch aggregation
	 */
	unsigned int ismerged:1;
	unsigned int isexec:1;

	unsigned int util:1;

	unsigned int mark:24;
	struct cvs_revision *prev;
	struct cvs_commit *cvs_commit;
	time_t timestamp;
};

#endif
