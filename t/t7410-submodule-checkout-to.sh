#!/bin/sh

test_description='Combination of submodules and multiple workdirs'

. ./test-lib.sh

base_path=$(pwd -P)

test_expect_success 'setup: make origin' \
    'mkdir -p origin/sub && ( cd origin/sub && git init &&
	test_commit sub_init file ) &&
    mkdir -p origin/sub2 && ( cd origin/sub2 && git init &&
	test_commit sub2_init file ) &&
    mkdir -p origin/main && ( cd origin/main && git init &&
	git submodule add ../sub &&
	git submodule add ../sub2 &&
	git commit -m "add submodules" ) &&
    ( cd origin/sub &&
	test_commit sub_update file ) &&
    ( cd origin/sub2 &&
	test_commit sub2_update file ) &&
    ( cd origin/main/sub && git pull ) &&
    ( cd origin/main/sub2 && git pull ) &&
    ( cd origin/main &&
	git add sub sub2 &&
	git commit -m "submodules updated" )'

test_expect_success 'setup: clone' \
    'mkdir clone && ( cd clone &&
	git clone --recursive "$base_path/origin/main")'

rev1_hash_main=$(git --git-dir=origin/main/.git show --pretty=format:%h -q "HEAD~1")
rev1_hash_sub=$(git --git-dir=origin/sub/.git show --pretty=format:%h -q "HEAD~1")

test_expect_success 'checkout main' \
    'mkdir default_checkout &&
    (cd clone/main &&
	git worktree add "$base_path/default_checkout/main" "$rev1_hash_main")'

test_expect_success 'cannot see submodule diffs just after checking out main' \
    '(cd default_checkout/main &&
	git diff --submodule master"^!" | grep "Submodule sub .*(not checked out)" &&
	git diff --submodule master"^!" | grep "Submodule sub2 .*(not checked out)")'

test_expect_success 'checkout main and initialize independed clones' \
    'mkdir fully_cloned_submodule &&
    (cd clone/main &&
	git worktree add "$base_path/fully_cloned_submodule/main" "$rev1_hash_main") &&
    (cd fully_cloned_submodule/main &&
	git submodule init sub &&
	git submodule update)'

test_expect_success 'can see submodule diffs after independed cloning' \
    '(cd fully_cloned_submodule/main && git diff --submodule master"^!" | grep "sub_update")'

test_expect_success 'sub2 remains uninitialized' '
    (cd fully_cloned_submodule/main &&
	git diff --submodule master"^!" | grep "Submodule sub2 .*(not checked out)")'

test_expect_success 'checkout sub manually' \
    'mkdir linked_submodule &&
    (cd clone/main &&
	git worktree add "$base_path/linked_submodule/main" "$rev1_hash_main") &&
    (cd clone/main/sub &&
	git worktree add "$base_path/linked_submodule/main/sub" "$rev1_hash_sub")'

test_expect_success 'can see submodule diffs after manual checkout of linked submodule' \
    '(cd linked_submodule/main && git diff --submodule master"^!" | grep "sub_update")'

test_done
