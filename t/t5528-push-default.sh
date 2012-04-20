#!/bin/sh

test_description='check various push.default settings'
. ./test-lib.sh

test_expect_success 'setup bare remotes' '
	git init --bare repo1 &&
	git remote add parent1 repo1 &&
	git init --bare repo2 &&
	git remote add parent2 repo2 &&
	test_commit one &&
	git push parent1 HEAD &&
	git push parent2 HEAD
'

# $1 = local revision
# $2 = remote repository
# $3 = remote revision (tested to be equal to the local one)
check_pushed_commit () {
	git rev-parse "$1" > expect &&
	git --git-dir="$2" rev-parse "$3" > actual &&
	test_cmp expect actual
}

# $1 = push.default value
# $2 = expected target branch for the push
test_push_success () {
	git -c push.default="$1" push &&
	check_pushed_commit HEAD repo1 "$2"
}

test_expect_success '"upstream" pushes to configured upstream' '
	git checkout master &&
	test_config branch.master.remote parent1 &&
	test_config branch.master.merge refs/heads/foo &&
	test_config push.default upstream &&
	test_commit two &&
	git push &&
	check_pushed_commit HEAD repo1 foo
'

test_expect_success '"upstream" does not push on unconfigured remote' '
	git checkout master &&
	test_unconfig branch.master.remote &&
	test_config push.default upstream &&
	test_commit three &&
	test_must_fail git push
'

test_expect_success '"upstream" does not push on unconfigured branch' '
	git checkout master &&
	test_config branch.master.remote parent1 &&
	test_unconfig branch.master.merge &&
	test_config push.default upstream
	test_commit four &&
	test_must_fail git push
'

test_expect_success '"upstream" does not push when remotes do not match' '
	git checkout master &&
	test_config branch.master.remote parent1 &&
	test_config branch.master.merge refs/heads/foo &&
	test_config push.default upstream &&
	test_commit five &&
	test_must_fail git push parent2
'

test_expect_success 'push from/to new branch with upstream, matching and simple' '
	git checkout -b new-branch &&
	test_must_fail git -c push.default=simple push &&
	test_must_fail git -c push.default=matching push &&
	test_must_fail git -c push.default=upstream push
'

test_expect_success 'push from/to new branch with current creates remote branch' '
	test_config branch.new-branch.remote repo1 &&
	git checkout new-branch &&
	test_push_success current new-branch
'

test_expect_success 'push to existing branch, with no upstream configured' '
	test_config branch.master.remote repo1 &&
	git checkout master &&
	test_must_fail git -c push.default=simple push &&
	test_must_fail git -c push.default=upstream push
'

test_expect_success 'push to existing branch, upstream configured with same name' '
	test_config branch.master.remote repo1 &&
	test_config branch.master.merge refs/heads/master &&
	git checkout master &&
	test_commit six &&
	test_push_success upstream master &&
	test_commit seven &&
	test_push_success simple master &&
	check_pushed_commit HEAD repo1 master
'

test_expect_success 'push to existing branch, upstream configured with different name' '
	test_config branch.master.remote repo1 &&
	test_config branch.master.merge refs/heads/other-name &&
	git checkout master &&
	test_commit eight &&
	test_push_success upstream other-name &&
	test_commit nine &&
	test_must_fail git -c push.default=simple push &&
	test_push_success current master &&
	test_must_fail check_pushed_commit HEAD repo1 other-name
'

test_done
