/*
 * Git submodule
 *
 * This is the submodule UI for git. The core submodule functionality can be
 * found in submodule.c (you are reading builtin/submodule.c).
 *
 * Copyright Fredrik Gustafsson <iveqy@iveqy.com>
 * See COPYING for license information.
 *
 */

/******** Includes ********/
/*{*/
#include "builtin.h"
#include "submodule.h"

#include <stdio.h>
/*}*/

/******** Private functions ********/
/*{*/

/**
 * \function
 * Find submodules that is in the current commit.
 *
 * @param limit_path to limit submodule list.
 * @return string_list of submodule paths.
 */
static string_list collect_submodules(const char *limit_path)
{ /*{*/
} /*}*/

/*}*/

/******** Public functions ********/
/*{*/

/**
 * \function
 * Take action depending on the arguments the user supplied to git submodule
 *
 * @param number of command line arguments
 * @param list of arguments
 * @param relative path from GITDIR, null if we are in GITDIR
 * @return 0 on success, otherwise <0
 */
int cmd_submodule(int argc, const char **argv, const char *prefix)
{ /*{*/
	printf("Prefix: %s\n", prefix);
	return 0;
} /*}*/

/*}*/
