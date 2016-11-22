#!/bin/sh

test_description='Test submodule embedgitdirs

This test verifies that `git submodue embedgitdirs` moves a submodules git
directory into the superproject.
'

. ./test-lib.sh

test_expect_success 'setup a real submodule' '
	git init sub1 &&
	test_commit -C sub1 first &&
	git submodule add ./sub1 &&
	test_tick &&
	git commit -m superproject
'

test_expect_success 'embed the git dir' '
	>expect.1 &&
	>expect.2 &&
	>actual.1 &&
	>actual.2 &&
	git status >expect.1 &&
	git -C sub1 rev-parse HEAD >expect.2 &&
	git submodule embedgitdirs &&
	git fsck &&
	test -f sub1/.git &&
	test -d .git/modules/sub1 &&
	git status >actual.1 &&
	git -C sub1 rev-parse HEAD >actual.2 &&
	test_cmp expect.1 actual.1 &&
	test_cmp expect.2 actual.2
'

test_expect_success 'setup nested submodule' '
	git init sub1/nested &&
	test_commit -C sub1/nested first_nested &&
	git -C sub1 submodule add ./nested &&
	test_tick &&
	git -C sub1 commit -m "add nested" &&
	git add sub1 &&
	git commit -m "sub1 to include nested submodule"
'

test_expect_success 'embed the git dir in a nested submodule' '
	git status >expect.1 &&
	git -C sub1/nested rev-parse HEAD >expect.2 &&
	git submodule embedgitdirs &&
	test -f sub1/nested/.git &&
	test -d .git/modules/sub1/modules/nested &&
	git status >actual.1 &&
	git -C sub1/nested rev-parse HEAD >actual.2 &&
	test_cmp expect.1 actual.1 &&
	test_cmp expect.2 actual.2
'

test_expect_success 'setup a gitlink with missing .gitmodules entry' '
	git init sub2 &&
	test_commit -C sub2 first &&
	git add sub2 &&
	git commit -m superproject
'

test_expect_success 'embedding the git dir fails for incomplete submodules' '
	git status >expect.1 &&
	git -C sub2 rev-parse HEAD >expect.2 &&
	test_must_fail git submodule embedgitdirs &&
	git -C sub2 fsck &&
	test -d sub2/.git &&
	git status >actual &&
	git -C sub2 rev-parse HEAD >actual.2 &&
	test_cmp expect.1 actual.1 &&
	test_cmp expect.2 actual.2
'

test_done
