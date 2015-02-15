#!/bin/sh

test_description='Test subdir-tool'

. ./test-lib.sh

test_expect_success 'setup' '
	mkdir -p foo/bar &&
	test_commit 1st foo/bar/baz
'

test_expect_success 'cannot extract top-level dir from tree (would be no-op)' '
	test_expect_code 2 git subdir-tool --extract / 1st:
'

test_expect_success 'extract subdir "foo" from tree' '
	git rev-parse 1st:foo/ >expect &&
	git subdir-tool --extract foo/ 1st: >actual &&
	test_cmp expect actual
'

test_expect_success 'cannot extract missing subdir "bar" from tree' '
	test_expect_code 3 git subdir-tool --extract bar/ 1st:
'

test_expect_success 'extract subdir "foo/bar" from tree' '
	git rev-parse 1st:foo/bar >expect &&
	git subdir-tool --extract foo/bar 1st: >actual &&
	test_cmp expect actual
'

test_expect_success 'extract subdir "foo" from commit' '
	git rev-parse 1st:foo/ >expect &&
	git subdir-tool --extract foo/ 1st >actual &&
	test_cmp expect actual
'

test_inserted_tree() {
	expect_tree=$(cat "$1")
	expect_subdir="$2"
	actual_tree=$(cat "$3")
	return 1
}

test_expect_success 'prepend subdir "spam" to tree' '
	git rev-parse 1st: >expect &&
	git subdir-tool --insert spam/ 1st: >actual &&
	test_inserted_tree expect spam actual
'

test_done
