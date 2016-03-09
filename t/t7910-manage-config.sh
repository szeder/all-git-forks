#!/bin/sh
# Copyright (c) 2014, Jonathan D. Simms, Twitter, Inc.

test_description='Test remote repo config management command "git manage-config".

'

. ./test-lib.sh

setup_update_sh() {
	cat <<-EOS
#!$SHELL_PATH
echo "exec-path: \$(git --exec-path)"
EOS
}

TEST_DIR=blah

assert_repo_dot_d_extracted() {
	test_path_is_dir .git/repo.d ".repo.d did not extract correctly" &&
	test_path_is_file .git/repo.d/update.sh "update.sh does not exist"
}

assert_no_repo_dot_d() {
	test_path_is_missing .git/repo.d ".repo.d extracted, should not"
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

test_expect_success PYTHON "should run update.sh if explicitly enabled" '
	TEST_DIR=explicit &&

	git clone . $TEST_DIR &&
	(
		cd $TEST_DIR &&
		git config --bool manageconfig.enable true &&
		git manage-config update >../actual &&
		assert_repo_dot_d_extracted
	) &&

	echo "exec-path: $(git --exec-path)" >expect &&
	test_cmp expect actual
'

test_expect_success PYTHON "should run update.sh if --force given" '
	TEST_DIR=force &&

	git clone . $TEST_DIR &&
	(
		cd $TEST_DIR &&
		git manage-config update --force >../actual &&
		assert_repo_dot_d_extracted
	) &&

	echo "exec-path: $(git --exec-path)" >expect &&
	test_cmp expect actual
'

setup_other_update_sh() {
	cat <<-EOS
#!$SHELL_PATH
echo "other update.sh"
EOS
}


test_expect_success PYTHON "should run if explicit ref given" '
	TEST_DIR=explicit-ref &&

	git clone . $TEST_DIR &&
	(
		cd $TEST_DIR &&
		git checkout -b local-repo.d origin/repo.d/master &&
		setup_other_update_sh > update.sh &&
		git commit -m "changed update.sh" -- update.sh &&
		git checkout master &&
		git manage-config update --use-ref=local-repo.d >../actual
	) &&

	echo "other update.sh" >expect &&
	test_cmp expect actual
'

test_expect_success PYTHON "should barf if --use-ref is not given an argument" '
	test_must_fail git manage-config update --use-ref
'

test_expect_success PYTHON "should not run if not whitelisted and --auto" '
	TEST_DIR=not-whitelist &&

	git clone . $TEST_DIR &&
	(
		cd $TEST_DIR &&
		git manage-config update --auto >../should-be-empty &&
		assert_no_repo_dot_d
	) &&

	test_line_count = 0 should-be-empty
'

test_expect_success PYTHON "should run if whitelisted and --auto" '
	TEST_DIR=whitelist-auto &&

	git clone . $TEST_DIR &&
	(
		cd $TEST_DIR &&
		git config remote.origin.url "https://git.twitter.biz/source" &&
		git manage-config update --auto >../actual &&
		assert_repo_dot_d_extracted
	) &&

	echo "exec-path: $(git --exec-path)" >expect &&
	test_cmp expect actual
'

test_expect_success PYTHON "should run if whitelisted and ro url" '
	TEST_DIR=whitelist-auto-git-ext &&

	git clone . $TEST_DIR &&
	(
		cd $TEST_DIR &&
		git config remote.origin.url "https://git.twitter.biz/ro/source" &&
		git manage-config update --auto >../actual &&
		assert_repo_dot_d_extracted
	) &&

	echo "exec-path: $(git --exec-path)" >expect &&
	test_cmp expect actual
'

test_expect_success PYTHON "should run if whitelisted and using the DNS ro redirect url" '
	TEST_DIR=whitelist-auto-dns-git-ro &&

	git clone . $TEST_DIR &&
	(
		cd $TEST_DIR &&
		git config remote.origin.url "https://git-ro-source.twitter.biz/source" &&
		git manage-config update --auto >../actual &&
		assert_repo_dot_d_extracted
	) &&

	echo "exec-path: $(git --exec-path)" >expect &&
	test_cmp expect actual
'

test_expect_success PYTHON "should run if whitelisted and url has a .git extension" '
	TEST_DIR=whitelist-auto-ro &&

	git clone . $TEST_DIR &&
	(
		cd $TEST_DIR &&
		git config remote.origin.url "https://git.twitter.biz/ro/source.git" &&
		git manage-config update --auto >../actual &&
		assert_repo_dot_d_extracted
	) &&

	echo "exec-path: $(git --exec-path)" >expect &&
	test_cmp expect actual
'


test_expect_success PYTHON "should not run if whitelist and explicitly disabled and --auto" '
	TEST_DIR=whitelist-disabled-auto &&

	git clone . $TEST_DIR &&
	(
		cd $TEST_DIR &&
		git config remote.origin.url "https://git.twitter.biz/source" &&
		git config --bool manageconfig.enable false &&
		git manage-config update --auto >../should-be-empty &&
		assert_no_repo_dot_d
	) &&

	test_line_count = 0 should-be-empty
'

test_expect_success PYTHON "should not run if whitelist and explicitly disabled" '
	TEST_DIR=whitelist-disabled &&

	git clone . $TEST_DIR &&
	(
		cd $TEST_DIR &&
		git config remote.origin.url "https://git.twitter.biz/source" &&
		git config --bool manageconfig.enable false &&
		git manage-config update >../should-be-empty &&
		assert_no_repo_dot_d
	) &&

	test_line_count = 0 should-be-empty
'

test_expect_success PYTHON "works when remote origin doesn't exist" '
	git manage-config update > should-be-empty &&
	test_line_count = 0 should-be-empty
'

test_done
