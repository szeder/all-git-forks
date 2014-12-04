#!/bin/sh

test_description='Regular activity for submodule (excluding adding, removing, initialization and deinitialization)'

. ./test-lib.sh

test_create_repo subsub
test_create_repo sub
test_create_repo super

sub_commit()
{
    sub_path="$1"
    sub_tag="$2"
    message="$3"

    (cd "$sub_path" && git checkout "$sub_tag") && \
    git add "$sub_path" && \
    git commit -m "$message" && \
    git tag "$message"
}

test_expect_success 'setup: make original' '
    ( cd subsub &&
	test_commit subsub_init file &&
	test_commit subsub_update file ) &&
    ( cd sub &&
	git submodule add ../subsub &&
	sub_commit subsub subsub_init sub_init &&
	sub_commit subsub subsub_update sub_update ) &&
    ( cd super &&
	git submodule add ../sub &&
	sub_commit sub sub_init super_init &&
	sub_commit sub sub_update super_update )'

test_expect_success 'make linked worktrees' '
    git --git-dir=super/.git checkout --to super-rec super_update &&
    git --git-dir=sub/.git checkout --to super-rec/sub sub_update &&
    git --git-dir=subsub/.git checkout --to super-rec/sub/subsub subsub_update &&
    git --git-dir=super/.git checkout --to super-mixed1 super_update &&
    git --git-dir=sub/.git checkout --to super-mixed1/sub sub_update &&
    git clone subsub super-mixed1/sub/subsub &&
    git --git-dir=super/.git checkout --to super-mixed2 super_update &&
    git clone sub super-mixed2/sub &&
    git --git-dir=subsub/.git checkout --to super-mixed2/sub/subsub subsub_update &&
    git --git-dir=super/.git checkout --to super-norec super_update'

test_expect_success 'inquire norec does not show anything' '
    ( cd super-norec && git diff --submodule HEAD~1 | grep -q "not checked out" )'

test_expect_success 'norec ignores update' '
    ( cd super-norec &&
	git reset --hard super_init && git submodule update --recursive &&
	test_dir_is_empty sub )'

test_expect_success 'inquire rec shows changes' '
    ( cd super-rec && git diff --submodule HEAD~1 | grep -q "sub_update" )'

test_expect_success 'recursive update works' '
    ( cd super-rec &&
	git reset --hard super_init && git submodule update --recursive &&
	echo subsub_init >../expected &&
	test_cmp ../expected sub/subsub/file )'

test_expect_success 'recursive update works in mixed1' '
    ( cd super-mixed1 &&
	git reset --hard super_init && git submodule update --recursive &&
	echo subsub_init >../expected &&
	test_cmp ../expected sub/subsub/file )'

test_expect_failure 'recursive update works in mixed2' '
    ( cd super-mixed2 &&
	git reset --hard super_init && git submodule update --recursive &&
	echo subsub_init >../expected &&
	test_cmp ../expected sub/subsub/file )'

test_expect_success 'recursive update works in mixed2 - set updateWithoutUrl' '
    test_when_finished "( cd super-mixed2/sub && git config --unset-all submodule.updateWithoutUrl )" &&
    ( cd super-mixed2/sub && git config submodule.updateWithoutUrl true ) &&
    ( cd super-mixed2 &&
	git submodule update --recursive &&
	echo subsub_init >../expected &&
	test_cmp ../expected sub/subsub/file )'

test_done
