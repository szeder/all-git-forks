#!/bin/sh
# Copyright (c) 2014, Jonathan D. Simms, Twitter, Inc.

test_description='Test remote repo config management command "git manage-config" called by fetch.

'

. ./test-lib.sh

HOOK_OUTPUT='@@HOOK@@: called update hook'

setup_update_sh() {
	exitstatus=${1:-0}
	cat <<-EOS
#!$SHELL_PATH
echo "$HOOK_OUTPUT"
exit $exitstatus
EOS
}

TEST_DIR=blah

assert_repo_dot_d_extracted() {
	test_path_is_dir .git/repo.d "repo.d did not extract correctly" &&
	test_path_is_file .git/repo.d/update.sh "update.sh does not exist"
}

assert_no_repo_dot_d() {
	test_path_is_missing .git/repo.d "repo.d extracted, should not"
}

test_expect_success setup '
	test_commit "initial-commit" file.txt "wut" &&
	git checkout --orphan repo.d/master &&
	git rm -rf . &&
	setup_update_sh >update.sh &&
	chmod 0755 update.sh &&
	git add update.sh &&
	git commit -a -m "repo.d-branch" &&
	git checkout master
'

# we can't do a whitelist --auto test b/c the whitelist is hardcoded to
# only allow https://git.twitter.biz/source as an origin

test_expect_success PYTHON "should run update.sh after fetch if explicitly enabled" '
	TEST_DIR=explicit &&

	git clone . $TEST_DIR &&
	(
		cd $TEST_DIR &&
		git config --bool manageconfig.enable true
	) &&

	test_commit "second-commit" two.txt "two" &&

	(
		cd $TEST_DIR &&
		git fetch origin | grep -e "@@HOOK@@" >../actual &&
		assert_repo_dot_d_extracted
	) &&

	echo "$HOOK_OUTPUT" >expect &&
	test_cmp expect actual
'

test_expect_success PYTHON "should not run update.sh after fetch if --disable-manage-config" '
	TEST_DIR=disable-flag &&

	git clone . $TEST_DIR &&
	(
		cd $TEST_DIR &&
		git config --bool manageconfig.enable true
	) &&

	test_commit "third-commit" three.txt "three" &&

	(
		cd $TEST_DIR &&
		git fetch --disable-manage-config origin | grep -e "@@HOOK@@" >../should-be-empty || true &&
		assert_no_repo_dot_d
	) &&

	test_line_count = 0 should-be-empty
'

test_expect_success PYTHON "nothing bad should happen if enabled and no repo.d/master" '
	TEST_DIR=nothing-bad &&

	git init second-remote &&
	(
		cd second-remote &&
		test_commit "one" one.txt "one"
	) &&

	git clone second-remote $TEST_DIR &&
	(
		cd second-remote &&
		test_commit "two" two.txt "two"
	) &&
	(
		cd $TEST_DIR &&
		git config --bool manageconfig.enable true &&
		git fetch origin &&
		assert_no_repo_dot_d
	)
'


test_done
