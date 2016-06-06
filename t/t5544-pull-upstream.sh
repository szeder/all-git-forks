#!/bin/sh

test_description='pull with --set-upstream'
. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-terminal.sh

test_config_unchanged () {
	git config --list --local >original
	"$@"
	git config --list --local >modified
	test_cmp original modified
}

check_config () {
	(echo "$2"; echo "$3") >expect
	(git config branch.$1.remote
	 git config branch.$1.merge) >actual
	test_cmp expect actual
}

test_expect_success 'setup repos' '
	git init parent &&
	(
		cd parent &&
		echo content >file &&
		git add file &&
		git commit -am one &&
		git tag initial_tag &&
		git checkout -b master2 &&
		echo content_modified >file &&
		git commit -am "file modification"
		git checkout -b other master &&
		echo content >file2 &&
		git add file2 &&
		git commit -am two &&
		git checkout -b other2
	) &&
	git init step_parent &&
	(
		cd step_parent &&
		echo step_content >step_file &&
		git add step_file &&
		git commit -m step_one
	) &&
	git remote add upstream parent &&
	git remote add step_upstream step_parent &&
	git pull upstream master &&
	git branch other
'

test_expect_success 'pull -u master' '
	git pull -u upstream master &&
	check_config master upstream refs/heads/master
'

test_expect_success 'pull -u takes the last branch as upstream' '
	test_might_fail git config --unset branch.master.merge &&
	test_might_fail git config --unset branch.master.remote &&
	git pull -u upstream master master2 &&
	check_config master upstream refs/heads/master2
'

test_expect_success 'pull -u master:other' '
	git pull -u upstream master:other &&
	check_config other upstream refs/heads/master
'


test_expect_success 'pull -u tracking non-local branch' '
	git checkout -b master2_local &&
	git pull -u upstream master2 &&
	check_config master2_local upstream refs/heads/master2
'


test_expect_success 'pull -u --dry-run other:other' '
	git config branch.other.merge refs/heads/master &&
	git config branch.other.remote upstream &&
	git pull -u --dry-run upstream other:other &&
	check_config other upstream refs/heads/master
'

test_expect_success 'pull -u master2:master2 master:other' '
	git branch master2 &&
	git pull -u upstream master2:master2 master:other &&
	check_config master2 upstream refs/heads/master2 &&
	check_config other upstream refs/heads/master
'

test_expect_success 'pull -u HEAD does not track origin/HEAD nor remote HEAD on origin' '
	git checkout -b other_head master &&
	git fetch upstream other &&
	git remote set-head upstream other &&
	test_config_unchanged git pull -u upstream HEAD
'

test_expect_success 'pull -u sets upstream when merge conflicts occur' '
	git checkout -b master_edited master &&
	echo conflict >file2 &&
	git add file2 &&
	git commit -am conflict &&
	test_must_fail git pull -u upstream other &&
	git rm file2 &&
	git commit &&
	check_config master_edited upstream refs/heads/other
'

test_expect_success 'pull -u should not work when merging unrelated histories' '
	git checkout master &&
	test_config_unchanged test_must_fail git pull -u step_parent master
'

test_expect_success 'pull -u sets upstream after rebasing' '
	git checkout -b other_rebased other &&
	git config branch.other_rebased.merge master &&
	ls .git/refs/remotes/upstream &&
	git pull -r -u upstream master2 &&
	git branch --set-upstream-to=upstream/master2 &&
	ls .git/refs/remotes/upstream &&
	check_config other_rebased upstream refs/heads/master2
'

test_expect_success 'pull -u refs/heads/*:refs/remotes/origin/* should not work' '
	git checkout master &&
	test_config_unchanged git pull -u upstream "refs/heads/*:refs/remotes/upstream/*"
'

test_expect_success 'pull -u master:refs/remotes/origin/master should not work' '
	test_config_unchanged git pull -u upstream master:refs/remotes/upstream/master
'

test_expect_success 'pull -u with a tag should not work' '
	git checkout master &&
	test_config_unchanged git pull -u upstream initial_tag
'

test_expect_success 'pull -u on detached head should not work' '
	git checkout HEAD^0 &&
	test_config_unchanged git pull -u upstream master2 &&
	git checkout -
'

test_expect_success 'pull -u with an unconfigured remote should not work' '
	git checkout master &&
	git clone parent unconfigured_parent &&
	test_config_unchanged git pull -u "$(pwd)/unconfigured_parent" other2
'

test_expect_success 'pull -u with a modified remote.fetch' '
	git remote add origin_modified parent &&
	git push upstream master:custom_branch &&
	git config remote.origin_modified.fetch "+refs/heads/*:refs/remotes/custom/*" &&
	git checkout -b lonely_branch master &&
	git pull -u origin_modified custom_branch &&
	check_config lonely_branch origin_modified refs/heads/custom_branch
'

test_expect_success TTY 'quiet pull' '
	git checkout master &&
	test_terminal git pull -u --quiet upstream master 2>&1 | tee output &&
	test_cmp /dev/null output
'

test_done
