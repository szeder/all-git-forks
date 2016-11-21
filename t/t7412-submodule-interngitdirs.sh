#!/bin/sh

test_description='Test submodule interngitdirs

This test verifies that `git submodue interngitdirs` moves a submodules git
directory into the superproject.
'

. ./test-lib.sh

test_expect_success 'setup a real submodule' '
	git init sub1 &&
	test_commit -C sub1 first &&
	git submodule add ./sub1 &&
	test_tick &&
	git commit -m superproject
'

test_expect_success 'intern the git dir' '
	git submodule interngitdirs &&
	test -f sub1/.git &&
	test -d .git/modules/sub1 &&
	# check that we did not break the repository:
	git status
'

test_expect_success 'setup a gitlink with missing .gitmodules entry' '
	git init sub2 &&
	test_commit -C sub2 first &&
	git add sub2 &&
	git commit -m superproject
'

test_expect_success 'intern the git dir fails for incomplete submodules' '
	test_must_fail git submodule interngitdirs &&
	# check that we did not break the repository:
	git status
'

test_done

