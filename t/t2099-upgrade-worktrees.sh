#!/bin/sh

test_description='Test upgrade of linked worktree from older format'

TEST_NO_CREATE_REPO=1
. ./test-lib.sh

test_expect_success 'untar' 'tar xzf ../t2025.tar.gz'

echo "path=$PATH"

old_trash=$(sed -e 's|gitdir: \(.*\)/\.git/[^\.]*$|\1|' here/.git)
new_trash=$(pwd)

test_expect_success 'patch' '
    grep -rl t2025 . | while read filename; do
	sed -i -e "s|$old_trash|$new_trash|g" "$filename"
    done
'

test_expect_success 'upgrade' '../../git-upgrade-worktrees . && ../../git-upgrade-worktrees bare'

test_expect_success 'checkout busy branch' '
    test_must_fail git checkout newmaster
'

wt_here_orig="$new_trash/here"
wt_root="$new_trash/wt_root"
wt_here="$new_trash/wt_here"

test_expect_success 'verify core.worktree' '
    printf "%s\n" "$wt_here_orig" >wt_here_expect &&
    ( cd here && git config core.worktree ) >wt_here_actual &&
    test_cmp wt_here_expect wt_here_actual
'

test_expect_success 'set variables' '
    mkdir -p "$wt_root" "$wt_here" &&
    git config config.var configvar &&
    git config core.worktree "$wt_root" &&
    ( cd here && git config core.worktree "$wt_here" )
'

test_expect_success 'verify variables' '
    echo configvar >configvar_expect &&
    git config config.var >configvar_actual &&
    printf "%s\n" "$wt_root" >wt_root_expect &&
    git config core.worktree >wt_root_actual &&
    printf "%s\n" "$wt_here" >wt_here_expect &&
    ( cd here && git config core.worktree ) >wt_here_actual &&
    ( cd here && git config config.var ) >configvar_here_actual &&
    test_cmp configvar_expect configvar_actual &&
    test_cmp configvar_expect configvar_here_actual &&
    test_cmp wt_here_expect wt_here_actual &&
    test_cmp wt_root_expect wt_root_actual
'

test_done
