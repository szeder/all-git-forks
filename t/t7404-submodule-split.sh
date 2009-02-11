#!/bin/sh
#
# Copyright 2009 Trustees of Dartmouth College

test_description='git submodule split tests'
. ./test-lib.sh

# We use two main repositories: An "original" repository, which remains
# unmodified, and a "working" repository, which we transform repeatedly.
rm -rf .git
test_create_repo original

test_expect_success 'create original repository' '
	(cd original &&
		echo "In main project" > main-file &&
		mkdir sub1 &&
		echo "In sub1" > sub1/sub1-file &&
		git add . &&
		git commit -m "Original project and sub1" &&
		git tag c1 &&
        	mkdir -p nested/sub2 &&
	  	echo "In sub2" > nested/sub2/sub2-file &&
		git add . &&
		git commit -m "Add sub2" &&
		git tag c2 &&
		git rm -r nested &&
		git commit -m "Removing nested temporarily" &&
		git tag c3 &&
		git checkout c2 -- nested &&
		git add . &&
		git commit -m "Putting nested back" &&
		git tag c4 &&
		git mv nested/sub2 other-sub2 &&
		echo "Changed file" >> other-sub2/sub2-file &&
		git add . &&
		git commit -m "Moving sub2 and changing a file" &&
		git tag c5 &&
		git mv other-sub2 nested/sub2 &&
		git commit -m "Moving sub2 back" &&
		git tag c6
	)
'

test_expect_success 'make a working repository' '
	abs_src_path="$(pwd)/original" && mkdir working &&
	(cd working &&
		git init &&
		git remote add origin --mirror "$abs_src_path" &&
		git fetch --update-head-ok &&
		git remote rm origin &&
		git read-tree -u -m HEAD)
'

test_expect_success 'split out sub1' '
	(cd working &&
		git submodule-split --url ../sub1-repo sub1 &&
		test -f main-file &&
		test -d sub1/.git &&
		test_must_fail git rev-parse -q --verify HEAD:sub1/sub1-file &&
		(cd sub1 && git rev-parse -q --verify HEAD:sub1-file)
	)
'

test_expect_success 'split out sub2' '
	(cd working &&
		rm -rf .git/refs/original &&
		git submodule-split nested/sub2 other-sub2 &&
		test -d nested/sub2/.git &&
		test_must_fail git rev-parse -q --verify \
			HEAD:nested/sub2/sub2-file &&
		test_must_fail git rev-parse -q --verify \
			c5:other-sub2/sub2-file &&
		(cd nested/sub2 &&
			git rev-parse -q --verify HEAD:sub2-file &&
			git rev-parse -q --verify c5:sub2-file)
	)
'

submodule_path() {
	git config -f .gitmodules submodule."$1".path
}

submodule_url() {
	git config -f .gitmodules submodule."$1".url
}

test_expect_success 'make sure .gitmodules knows about both submodules' '
	(cd working &&
		test "$(submodule_path sub1)" = sub1 &&
		test "$(submodule_url  sub1)" = ../sub1-repo &&
		test "$(submodule_path nested/sub2)" = nested/sub2 &&
		test "$(submodule_url  nested/sub2)" = ../nested/sub2
	)
'

test_expect_success 'compare each commit in split repository with original' '
	rm -rf working/.git/refs/original &&
	module_base="$(pwd)/original" &&
	(cd working && git config remote.origin.url "$module_base") &&
	mv working/sub1 sub1-repo &&
	mkdir nested && mv working/nested/sub2 nested &&
	original_revs="$(cd original && git rev-parse --all)" &&
	working_revs="$(cd working && git rev-parse --all)" &&
	while test -n "$original_revs"; do
		original_commit="$(echo "$original_revs" | head -n 1)" &&
		working_commit="$(echo "$working_revs" | head -n 1)" &&
		original_revs="$(echo "$original_revs" | tail -n +2)" &&
		working_revs="$(echo "$working_revs" | tail -n +2)" &&
		(cd original && git checkout -f "$original_commit") &&
		(cd working && git checkout -f "$working_commit" &&
			git clean -fd &&
			git submodule update --init) &&
		diff -Nr -x .git -x .gitmodules original working ||
			exit
	done
'

test_expect_success 'verify that empty commits are skipped' '
	(cd working/sub1 &&
		test "$(git rev-parse c1)" = "$(git rev-parse c2)"
	)
'

# Note that we should probably also drop the c3 tag here, because sub2
# temporarily disappeared from the tree during that commit, but doing so
# will require more work.  For now, we map c3 back to the last known state
# of the directory when it was actually in-tree.
test_expect_success 'verify that directories missing from rev are skipped' '
	(cd working/nested/sub2 &&
		test_must_fail git rev-parse -q --verify c1 &&
		test "$(git rev-parse c2)" = "$(git rev-parse c4)"
	)
'

test_done
