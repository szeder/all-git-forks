#!/bin/sh

test_description='cherry-pick should rerere for conflicts'

. ./test-lib.sh

test_expect_success setup '
	echo foo >foo &&
	git add foo && test_tick && git commit -q -m 1 &&
	echo foo-master >foo &&
	git add foo && test_tick && git commit -q -m 2 &&

	git checkout -b dev HEAD^ &&
	echo foo-dev >foo &&
	git add foo && test_tick && git commit -q -m 3 &&
	git config rerere.enabled true
'

test_expect_success 'conflicting merge' '
	test_must_fail git merge master
'

test_expect_success 'fixup' '
	echo foo-dev >foo &&
	git add foo && test_tick && git commit -q -m 4 &&
	git reset --hard HEAD^ &&
	echo foo-dev >expect
'

test_expect_success 'cherry-pick conflict' '
	test_must_fail git cherry-pick master &&
	test_cmp expect foo
'

test_expect_success 'reconfigure' '
	git config rerere.enabled false &&
	git reset --hard
'

test_expect_success 'cherry-pick conflict without rerere' '
	test_must_fail git cherry-pick master &&
	test_must_fail test_cmp expect foo
'

test_expect_success 'cherry-pick --rerere-autoupdate' '
	test_when_finished "rm -rf rerere" &&
	(
	git init rerere &&
	cd rerere &&
	test_config rerere.enabled true &&
	echo one > content-a && git add content-a &&
	echo one > content-b && git add content-b &&
	git commit -m one &&
	git checkout -b conflict master &&
	echo conflict-a > content-a &&
	git commit -a -m conflict-a &&
	echo conflict-b > content-b &&
	git commit -a -m conflict-b &&
	git checkout master &&
	echo two > content-a &&
	echo two > content-b &&
	git commit -a -m two &&
	git checkout -b test &&
	test_must_fail git cherry-pick master..conflict &&
	echo resolved-a > content-a &&
	git add content-a &&
	test_must_fail git cherry-pick --continue &&
	echo resolved-b > content-b &&
	git add content-b &&
	git cherry-pick --continue &&
	git reset --hard master &&
	git log --oneline --all --decorate --graph &&
	test_must_fail git cherry-pick --rerere-autoupdate master..conflict &&
	git log --oneline --all --decorate --graph &&
	echo resolved-a > expected &&
	test_cmp expected content-a
	test_must_fail git cherry-pick --continue &&
	echo resolved-b > expected &&
	test_cmp expected content-b &&
	git cherry-pick --continue
	)
'

test_done
