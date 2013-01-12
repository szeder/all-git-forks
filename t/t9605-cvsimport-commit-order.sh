#!/bin/sh

test_description='git cvsimport commit order'
. ./lib-cvs.sh

setup_cvs_test_repository t9605

test_expect_success 'checkout with CVS' '

	echo CVSROOT=$CVSROOT &&
	cvs checkout -d module-cvs module
'

test_expect_failure 'import into git (commit order mangled)' '

	git cvsimport -R -a -p"-x" -C module-git module &&
	(
		cd module-git &&
		git merge origin
	) &&
	test_cmp module-cvs/c module-git/c
'

test_done
