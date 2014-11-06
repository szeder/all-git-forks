#!/bin/sh

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
            git --no-pager log --oneline --graph --decorate "$commit" "$expected"
            echo "($commit_name) $commit != ($next) $expected"
            return 1
        fi
    done
}

# a---b---c             ONE
#          \
#           g           TWO
#            \
#             h         THREE
#
test_expect_success 'rebase two linear branches' '
    reset_rebase &&
    git branch -f ONE c &&
    git branch -f TWO g &&
    git branch -f THREE h &&
    git rebase ONE TWO THREE &&
    test_commits_equal c ONE TWO~1 THREE~2
    '

#
#           c           THREE
#          /
# a---b---d             ONE
#          \
#           e---f       TWO
#
test_expect_success 'rebase two forked branches preserving branches' '
    reset_rebase &&
    git branch -f ONE d &&
    git branch -f TWO f &&
    git branch -f THREE c &&
    git rebase -p ONE TWO THREE &&
    test_commits_equal d ONE TWO~2 THREE~1
    '

#
# a---b---d             ONE
#          \
#           e---f       TWO
#                \
#                 c     THREE
#
test_expect_success 'rebase two forked branches TWO THREE' '
    reset_rebase &&
    git branch -f ONE d &&
    git branch -f TWO f &&
    git branch -f THREE c &&
    git rebase ONE TWO THREE &&
    git --no-pager log --oneline --graph --decorate ONE TWO THREE &&
    test_commits_equal d ONE TWO~2 THREE~3
    '

#
# a---b---d             ONE
#          \
#           c           THREE
#            \
#             e---f     TWO
#
test_expect_success 'rebase two forked branches THREE TWO' '
    reset_rebase &&
    git branch -f ONE d &&
    git branch -f TWO f &&
    git branch -f THREE c &&
    git rebase ONE THREE TWO &&
    test_commits_equal d ONE TWO~3 THREE~1
    '

#
#           c           TWO
#          /
# a---d---b             ONE
#          \
#           e---f       THREE

test_expect_success 'swap fork preserving branches' '
    reset_rebase &&
    git branch -f ONE d &&
    git branch -f TWO c &&
    git branch -f THREE f &&
    git --no-pager log --oneline --graph --decorate ONE TWO THREE &&
    set_fake_editor &&
    FAKE_LINES="2 1 3 4 5" git rebase -i -p a ONE TWO THREE &&
    git --no-pager log --oneline --graph --decorate ONE TWO THREE
    '

test_done
