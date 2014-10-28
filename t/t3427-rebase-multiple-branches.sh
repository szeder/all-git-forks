#!/bin/sh

dbg=/tmp/test

test_description='basic multi-branch rebase tests'
. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-rebase.sh

# TODO: Poke around t3421 and t3410

#
#       c
#      /
# a---b---d
#  \   \
#   \   e---f
#    \       \
#     \   h   j---k
#      \ /   /
#       g---i
#
test_expect_success 'setup' '
	test_commit a file_0 &&
	test_commit b file_1 &&
	test_commit c file_2 &&
	git checkout b &&
	test_commit d file_3 &&
	git checkout b &&
	test_commit e file_4 &&
	test_commit f file_5 &&
	git checkout a &&
	test_commit g file_6 &&
	test_commit h file_7 &&
	git checkout g &&
	test_commit i file_8 &&
	git checkout f &&
	git merge i &&
	git tag j &&
	test_commit k file_9
'

# Useful for testing. Replace with test_cmp_rev and/or test_linear_range
test_commits_equal() {
	commit_name=$1
	commit=$(git rev-parse $1)
	shift
	for next in "$@"; do
		expected=$(git rev-parse $next)
		if [ "$commit" != "$expected" ]; then
			echo >> ${dbg}
			git --no-pager log --oneline --graph --decorate --all >> ${dbg}
			echo "($commit_name) $commit != ($next) $expected" >> ${dbg}
			echo >> ${dbg}
			return 1
		fi
	done
	return 0
}

# This is mostly here to just test that the testing framework is doing what I expected
test_expect_success 'simple rebase' '
	reset_rebase &&
	git checkout f &&
	git rebase d &&
	test_commits_equal HEAD~2 d &&
	test_path_is_file file_0 &&
	test_path_is_file file_1 &&
	test_path_is_file file_4 &&
	test_path_is_file file_5
	'

# Rebase branch. Left of branch rebased, right unaffected
test_expect_success 'branched rebase' '
	reset_rebase &&
	git checkout k &&
	git rebase -p d &&
	test_commits_equal HEAD~1^1~2 d &&
	test_commits_equal HEAD~1^2 i &&
	test_path_is_file file_3
	'

# a---b---c      ONE
#          \
#           g    TWO
#            \
#             h  THREE
#
test_expect_success 'merge two branches' '
	reset_rebase &&
	git branch -f ONE c &&
	git branch -f TWO g &&
	git branch -f THREE h &&
	git rebase -p ONE TWO THREE &&
	test_commits_equal c ONE TWO~1 THREE~2 &&
	echo
	'

test_done
