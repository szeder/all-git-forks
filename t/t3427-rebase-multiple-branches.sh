#!/bin/sh

test_description='basic multi-branch rebase tests'
. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-rebase.sh

# TODO: Poke around t3421 and t3410

#         e2
#        /
#       c---dp
#      /
# a---b---d
#  \   \
#   \   e---f
#    \       \
#     \   h   fi--k
#      \ /   /
#       g---i
#
test_expect_success 'setup' '
    test_commit a file_a &&
    test_commit b file_b &&
    test_commit c file_c &&
    test_commit dp file_d d &&
    git checkout c &&
    test_commit e2 file_e &&
    git checkout b &&
    test_commit d file_d &&
    git checkout b &&
    test_commit e file_e &&
    test_commit f file_f &&
    git checkout a &&
    test_commit g file_g &&
    test_commit h file_h &&
    git checkout g &&
    test_commit i file_i &&
    git checkout f &&
    git merge i &&
    git tag fi &&
    test_commit k file_k
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
# a--(b)--d             ONE
#  \   \
#   \   e---f
#    \       \
#     \       fi--k     TWO
#      \     /
#       g--(i)
#
test_expect_success 'remove commits preserving branches' '
    reset_rebase &&
    git branch -f ONE d &&
    git branch -f TWO k &&
    set_fake_editor &&
    FAKE_LINES="  2 3 4 5  7 8" git rebase -i -p a ONE TWO &&
    test_commits_equal a ONE~ TWO~4 TWO~^2~
    '

#
# a---d---b             ONE
#          \
#           c           TWO (dp gets skipped because d is now an ancestor)
#            \
#             e2        THREE
#
test_expect_success 'swap commit with multiple children preserving branches' '
    reset_rebase &&
    git branch -f ONE d &&
    git branch -f TWO dp &&
    git branch -f THREE e2 &&
    set_fake_editor &&
    export FAKE_LINES="2 1 3 4 6 5 7 8" &&
    test_must_fail git rebase -i -p a ONE TWO THREE &&
    git rebase --skip &&
    test_commits_equal a ONE~2 TWO~3 THREE~4 &&
    test_commits_equal ONE TWO~ THREE~2
    '

test_done

#
# a---d<->b             ONE
#  \       \
#   \       e---f
#    \           \
#     \           fi--k TWO
#      \         /
#       i<->g----
#
test_expect_success 'swap commits preserving branches' '
    reset_rebase &&
    git branch -f ONE d &&
    git branch -f TWO k &&
    set_fake_editor &&
    FAKE_LINES="2 1 3 4 6 5 7 8" git rebase -i -p a ONE TWO &&
    git --no-pager log --oneline --graph --decorate ONE TWO
    '

test_done
