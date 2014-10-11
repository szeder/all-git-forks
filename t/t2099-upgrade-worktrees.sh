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

test_done
