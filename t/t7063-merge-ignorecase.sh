#!/bin/sh

test_description='git-merge with case-changing rename on case-insensitive file system'

. ./test-lib.sh

if ! test_have_prereq CASE_INSENSITIVE_FS
then
	skip_all='skipping case insensitive tests - case sensitive file system'
	test_done
fi

test_expect_success 'merge with case-changing rename with ignorecase=true' '
	test $(git config core.ignorecase) = true &&
	touch TestCase &&
	git add TestCase &&
	git commit -m "add TestCase" &&
	git checkout -b with-camel &&
	touch foo &&
	git add foo &&
	git commit -m "intervening commit" &&
	git checkout master &&
	git rm TestCase &&
	touch testcase &&
	git add testcase &&
	git commit -m "rename to testcase" &&
	git checkout with-camel &&
	git merge master -m "merge" &&
	test -e testcase
'

test_done
