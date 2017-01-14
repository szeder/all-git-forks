#!/bin/sh
#
# Copyright (c) 2017 Wieland Hoffmann
#

test_description='test signoff'

. ./test-lib.sh

do_merge () {
	git reset --hard c0 &&
	git merge --signoff "$@" -m "merge modified" modified
}

verify_merge () {
	do_merge "$@" &&
	git cat-file commit HEAD | sed -e "1,/^\$/d" > actual &&
	(
		echo merge modified
		echo
		git var GIT_COMMITTER_IDENT |
		sed -e "s/>.*/>/" -e "s/^/Signed-off-by: /"
	) >expected &&
	test_cmp expected actual
}

test_expect_success 'setup' '
	test_commit foo file1 &&
	git tag c0 &&
	git checkout -b modified &&
	test_commit bar file1 &&
	git checkout master'

test_expect_success 'combining --signoff and --no-commit is refused' '
	test_must_fail do_merge --signoff --no-commit modified &&
	test_must_fail do_merge --no-commit --signoff modified'

test_expect_success 'combining --signoff and --ff-only is refused' '
	test_must_fail do_merge --signoff --ff-only modified &&
	test_must_fail do_merge --ff-only --signoff modified'

test_expect_success 'combining --signoff and --ff is refused' '
	test_must_fail do_merge --signoff --ff modified &&
	test_must_fail do_merge --ff --signoff modified'

test_expect_success 'combining --signoff and no ff option is refused' '
	test_must_fail do_merge --signoff modified'

test_expect_success '--signoff adds the Signed-off-by line' '
	verify_merge --commit --no-ff &&
	verify_merge --no-ff
'

test_done
