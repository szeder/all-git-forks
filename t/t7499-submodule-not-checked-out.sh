#!/bin/sh

test_description='Test not-checked-out submodules wrt cherrypicking and merging'

. ./test-lib.sh

test_expect_success 'setup origin' '
    test_create_repo origin &&
    test_create_repo origin/sub &&
    (
        cd origin/sub &&
        git commit --allow-empty -m "submodule commit 1" &&
        git tag submodule_commit_1 &&
        git rev-parse HEAD >../../submodule_commit_1_hash &&
        git commit --allow-empty -m "submodule commit 2" &&
        git tag submodule_commit_2 &&
        git rev-parse HEAD >../../submodule_commit_2_hash
    ) &&
    (
        cd origin &&
        ( cd sub && git reset --hard submodule_commit_1 ) &&
        git add sub &&
        git commit -m "main init" &&
        git tag main_initial &&

        touch bogus_file_in_unchanged_branch &&
        git add bogus_file_in_unchanged_branch &&
        git commit -m "main, submodule is unchanged" &&
        git tag main_sub_is_unchanged &&

        git reset --hard main_initial &&
        ( cd sub && git reset --hard submodule_commit_2 ) &&
        git add sub &&
        touch bogus_file_in_changed_branch &&
        git add bogus_file_in_changed_branch &&
        git commit -m "main, submodule is changed" &&
        git tag main_sub_is_changed
    )
'

test_expect_success 'setup: clone and verify that submodule is not checked out' '
    git clone origin cloned &&
    test ! -e cloned/sub/.git
'

test_expect_success 'cherry-pick unchanged to changed' '
    (
        cd cloned &&
        git reset --hard main_sub_is_changed &&
        git cherry-pick --no-edit main_sub_is_unchanged &&
        git rev-parse HEAD:sub >sub_hash &&
        test_cmp sub_hash ../submodule_commit_2_hash
    )
'

test_expect_success 'cherry-pick changed to unchanged' '
    (
        cd cloned &&
        git reset --hard main_sub_is_unchanged &&
        git cherry-pick --no-edit main_sub_is_changed &&
        git rev-parse HEAD:sub >sub_hash &&
        test_cmp sub_hash ../submodule_commit_2_hash
    )
'

test_expect_success 'merge unchanged to changed' '
    (
        cd cloned &&
        git reset --hard main_sub_is_changed &&
        git merge --no-edit main_sub_is_unchanged &&
        git rev-parse HEAD:sub >sub_hash &&
        test_cmp sub_hash ../submodule_commit_2_hash
    )
'

test_expect_success 'merge changed to unchanged' '
    (
        cd cloned &&
        git reset --hard main_sub_is_unchanged &&
        git merge --no-edit main_sub_is_changed &&
        git rev-parse HEAD:sub >sub_hash &&
        test_cmp sub_hash ../submodule_commit_2_hash
    )
'

test_done
