#!/bin/sh

test_description='Combination of submodules and multiple workdirs'

. ./test-lib.sh

base_path=$(pwd -P)

test_expect_success 'setup: make origin' \
    'mkdir -p origin/sub && ( cd origin/sub && git init &&
	echo file1 >file1 &&
	git add file1 &&
	git commit -m file1 ) &&
    mkdir -p origin/sub2 && ( cd origin/sub2 && git init &&
	echo file2 >file2 &&
	git add file2 &&
	git commit -m file2 ) &&
    mkdir -p origin/main && ( cd origin/main && git init &&
	git submodule add ../sub &&
	git commit -m "add sub" &&
	git tag main_add_sub &&
	(cd sub &&
	    echo file1updated >file1 &&
	    git commit -m "file1 updated" file1 &&
	    git push ../../sub HEAD:refs/tags/sub_head ) &&
	git commit -m "sub updated" sub &&
	git tag sub_updated &&
	git submodule deinit sub &&
	git rm sub &&
	git submodule add --name sub2 ../sub2 sub &&
	git commit -m "use sub2" &&
	(cd sub &&
	    echo file2updated >file2 &&
	    git commit -m "file2 updated" file2 &&
	    git push ../../sub2 HEAD:refs/tags/sub2_head ) &&
	git commit -m "sub2 updated" sub &&
	git tag sub2_updated )'

test_expect_success false false

test_expect_success 'setup: clone' \
    'mkdir clone && ( cd clone &&
	git clone --recursive "$base_path/origin/main")'

test_expect_success 'checkout main' \
    'mkdir default_checkout &&
    (cd clone/main &&
	git checkout --to "$base_path/default_checkout/main" "$rev1_hash_main")'

test_expect_failure 'can see submodule diffs just after checkout' \
    '(cd default_checkout/main && git diff --submodule master"^!" | grep "file1 updated")'

test_expect_success 'checkout main and initialize independed clones' \
    'mkdir fully_cloned_submodule &&
    (cd clone/main &&
	git checkout --to "$base_path/fully_cloned_submodule/main" "$rev1_hash_main") &&
    (cd fully_cloned_submodule/main && git submodule update)'

test_expect_success 'can see submodule diffs after independed cloning' \
    '(cd fully_cloned_submodule/main && git diff --submodule master"^!" | grep "file1 updated")'

test_expect_success 'checkout sub manually' \
    'mkdir linked_submodule &&
    (cd clone/main &&
	git checkout --to "$base_path/linked_submodule/main" "$rev1_hash_main") &&
    (cd clone/main/sub &&
	git checkout --to "$base_path/linked_submodule/main/sub" "$rev1_hash_sub") &&
    mkdir clone/main/.git/worktrees/main/modules &&
	echo "gitdir: ../../modules/sub/worktrees/sub" > clone/main/.git/worktrees/main/modules/sub'

test_expect_success 'can see submodule diffs after manual checkout of linked submodule' \
    '(cd linked_submodule/main && git diff --submodule master"^!" | grep "file1 updated")'

test_done
