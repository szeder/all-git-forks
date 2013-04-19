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
	test_commit my_master &&
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

test_expect_success 'checkout of branch from multiple remotes fails #1' '
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

test_expect_success 'setup more remotes with unconventional refspecs' '
	git checkout master &&
	git branch -D bar &&
	git branch -D baz &&
	test "$(git rev-parse --verify HEAD)" = "$(git rev-parse --verify my_master)" &&
	(git init repo_c &&
	 cd repo_c &&
	 test_commit c_master &&
	 git checkout -b bar &&
	 test_commit c_bar
	 git checkout -b spam &&
	 test_commit c_spam
	) &&
	(git init repo_d &&
	 cd repo_d &&
	 test_commit d_master &&
	 git checkout -b baz &&
	 test_commit f_baz
	 git checkout -b eggs &&
	 test_commit c_eggs
	) &&
	git remote add repo_c repo_c &&
	git config remote.repo_c.fetch \
	    "+refs/heads/*:refs/remotes/extra_dir/repo_c/extra_dir/*" &&
	git fetch repo_c &&
	git remote add repo_d repo_d &&
	git config remote.repo_d.fetch \
	    "+refs/heads/*:refs/repo_d/*" &&
	git fetch repo_d
'

test_expect_success 'checkout of branch from multiple remotes fails #2' '
	test_must_fail git checkout bar
'

test_expect_success 'checkout of branch from multiple remotes fails #3' '
	test_must_fail git checkout baz
'

test_expect_success 'checkout of branch from a single remote succeeds #3' '
	git checkout spam &&
	test_tracking_branch spam repo_c refs/remotes/extra_dir/repo_c/extra_dir/spam
'

test_expect_failure 'checkout of branch from a single remote succeeds #4' '
	git checkout eggs &&
	test_tracking_branch eggs repo_d refs/repo_d/eggs
'

test_done
