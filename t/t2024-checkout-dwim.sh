#!/bin/sh

test_description='checkout <branch>

Ensures that checkout on an unborn branch does what the user expects'

. ./test-lib.sh

# Arguments: <branch> <remote> <remote-tracking>
#
# Verify that we have checked out <branch>, and that it is at the same
# commit as <remote-tracking>, and that has appropriate tracking config
# setup against <remote>
test_tracking_branch() {
	branch=$1 &&
	remote=$2 &&
	remote_track=$3 &&
	test "refs/heads/$branch" = "$(git rev-parse --symbolic-full-name HEAD)" &&
	test "$(git rev-parse --verify HEAD)" = "$(git rev-parse --verify "$remote_track")" &&
	test "$remote" = "$(git config "branch.$branch.remote")" &&
	test "refs/heads/$branch" = "$(git config "branch.$branch.merge")"
}

test_expect_success 'setup' '
	(git init repo_a &&
	 cd repo_a &&
	 test_commit a_master &&
	 git checkout -b foo &&
	 test_commit a_foo &&
	 git checkout -b bar &&
	 test_commit a_bar
	) &&
	(git init repo_b &&
	 cd repo_b &&
	 test_commit b_master &&
	 git checkout -b foo &&
	 test_commit b_foo &&
	 git checkout -b baz &&
	 test_commit b_baz
	) &&
	git remote add repo_a repo_a &&
	git remote add repo_b repo_b &&
	git config remote.repo_b.fetch \
		"+refs/heads/*:refs/remotes/other_b/*" &&
	git fetch --all
'

test_expect_success 'checkout of non-existing branch fails' '
	test_must_fail git checkout xyzzy
'

test_expect_success 'checkout of branch from multiple remotes fails' '
	test_must_fail git checkout foo
'

test_expect_success 'checkout of branch from a single remote succeeds #1' '
	git checkout bar &&
	test_tracking_branch bar repo_a refs/remotes/repo_a/bar
'

test_expect_success 'checkout of branch from a single remote succeeds #2' '
	git checkout baz &&
	test_tracking_branch baz repo_b refs/remotes/other_b/baz
'

test_done
