#!/bin/sh

test_description='test push of submodules works correctly'

. ./test-lib.sh

test_expect_success setup '
	mkdir pub.git &&
	GIT_DIR=pub.git git init --bare
	GIT_DIR=pub.git git config receive.fsckobjects true &&
	mkdir work &&
	(
		cd work &&
		git init &&
		mkdir -p gar/bage &&
		(
			cd gar/bage &&
			git init &&
			>junk &&
			git add junk &&
			git commit -m "Initial junk"
		) &&
		git add gar/bage &&
		git commit -m "Initial superproject"
	)
'

test_expect_failure 'push superproject with not pushed subproject' '
	(
		cd work &&
		test_must_fail git push ../pub.git master &&
		git push -f ../pub.git master
	)
'

test_expect_failure 'push superproject with pushed subproject' '
	(
		cd work/gar/bage &&
		> junk2 &&
		git add junk2 &&
		git commit -m "More Junk" &&
		git push ../../../../sub.git
	) &&
	(
		cd work &&
		git add gar/bage &&
		git commit -m "More superproject" &&
		git push ../pub.git master
	)
'

test_done
