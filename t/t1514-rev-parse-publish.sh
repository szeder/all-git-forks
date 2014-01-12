#!/bin/sh

test_description='test <branch>@{publish} syntax'

. ./test-lib.sh

test_expect_success 'setup bare remotes' '
	git init --bare repo1 &&
	git remote add parent1 repo1 &&
	git init --bare repo2 &&
	git remote add parent2 repo2 &&
	test_commit one &&
	git push parent2 master &&
	test_commit two &&
	git push -u parent1 master &&
	git branch side &&
	git config branch.side.pushremote parent2 &&
	git branch side2
'

full_name () {
	(cd clone &&
	 git rev-parse --symbolic-full-name "$@")
}


test_expect_success '@{publish} resolves to correct full name' '
	test refs/remotes/parent1/master = "$(full_name @{publish})"
'

test_expect_success '@{p} resolves to correct full name' '
	test refs/remotes/parent1/master = "$(full_name @{p})"
'

test_expect_success '@{p} for branch with pushremote' '
	test refs/remotes/parent2/master = "$(full_name side@{p})"
'

test_expect_success '@{p} for branch without pushremote' '
	test refs/remotes/parent1/master = "$(full_name side2@{p})"
'

test_expect_success '@{p} with pushdefault' '
	test_when_finished "git config --unset remote.pushdefault" &&
	git config remote.pushdefault parent2
	test refs/remotes/parent2/master = "$(full_name @{p})"
'
