#!/bin/sh

test_description='check input limits for pushing'
. ./test-lib.sh

test_expect_success 'create known-size commit' '
	test-genrandom foo 1024 >file &&
	git add file &&
	test_commit one-k
'

test_expect_success 'create remote repository' '
	git init --bare dest &&
	git --git-dir=dest config receive.unpacklimit 1
'

test_expect_success 'receive.maxsize rejects push' '
	git --git-dir=dest config receive.maxsize 512 &&
	test_must_fail git push dest HEAD
'

test_expect_success 'bumping limit allows push' '
	git --git-dir=dest config receive.maxsize 4k &&
	git push dest HEAD
'

test_expect_success 'create another known-size commit' '
	test-genrandom bar 2048 >file2 &&
	git add file2 &&
	test_commit two-k
'

test_expect_success 'change unpacklimit' '
	git --git-dir=dest config receive.unpacklimit 10
'

test_expect_success 'receive.maxsize rejects push' '
	git --git-dir=dest config receive.maxsize 512 &&
	test_must_fail git push dest HEAD
'

test_expect_success 'bumping limit allows push' '
	git --git-dir=dest config receive.maxsize 4k &&
	git push dest HEAD
'

test_done
