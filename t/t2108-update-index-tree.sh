#!/bin/sh

test_description="update tree entries in index"

. ./test-lib.sh

test_expect_success 'add an empty tree' '
	tree_sha1=$(git write-tree) &&
	cat >expected <<-EOF &&
	040000 $tree_sha1 0	empty-dir
	EOF
	git update-index --add --index-info <expected &&
	git ls-files --stage >actual &&
	test_cmp expected actual
'

test_expect_success 'update-index --refresh' '
	git update-index --refresh
'

test_expect_success 'checkout' '
	git checkout-index empty-dir 2>err &&
	test_must_be_empty err
'

test_done
