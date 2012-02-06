#!/bin/sh

test_description='Narrow pathspec rewrite tests'

. ./test-lib.sh

test_expect_success setup '
	mkdir a b c &&
	mkdir a/aa a/ab a/ac &&
	mkdir b/ba b/bb b/bc &&
	mkdir a/aa/aaa a/aa/aab a/aa/aac &&
	mkdir a/ab/aba a/ab/abb a/ab/abc
'

test_expect_success '() no pathspec' '
	test-get-pathspec >result &&
	: >expected &&
	test_cmp expected result
'

test_expect_success '() [a] no pathspec' '
	echo a >.git/narrow &&
	test-get-pathspec >result &&
	echo a >expected &&
	test_cmp expected result
'

# Because narrow prefix is "a". put a/ to check that the prefix is
# actually from command line not from narrow prefix
test_expect_success '() [a] a/' '
	echo a >.git/narrow &&
	test-get-pathspec a/ >result &&
	echo a/ >expected &&
	test_cmp expected result
'

test_expect_success '() [a] a/aa' '
	echo a >.git/narrow &&
	test-get-pathspec a/aa >result &&
	echo a/aa >expected &&
	test_cmp expected result
'

test_expect_success '() [a] b' '
	echo a >.git/narrow &&
	test_must_fail test-get-pathspec b
'

test_expect_failure '() [a/aa] a' '
	echo a/aa >.git/narrow &&
	test-get-pathspec a >result &&
	echo a/aa >expected &&
	test_cmp expected result
'

test_expect_success '() [a/aa] a/ab' '
	echo a/aa >.git/narrow &&
	test_must_fail test-get-pathspec a/ab
'

test_expect_failure '() [a/aa a/ab] a' '
	echo a/aa >.git/narrow &&
	echo a/ab >>.git/narrow &&
	test-get-pathspec a >result &&
	echo a/aa >expected &&
	echo a/ab >>expected &&
	test_cmp expected result
'

test_expect_failure '() [a/aa a/ab] a a/aa/aab' '
	echo a/aa >.git/narrow &&
	echo a/ab >>.git/narrow &&
	test-get-pathspec a a/aa/aab >result &&
	echo a/aa >expected &&
	echo a/ab >>expected &&
	test_cmp expected result
'

test_expect_success '() [a/aa a/ab] a/aa a/ab/abc' '
	echo a/aa >.git/narrow &&
	echo a/ab >>.git/narrow &&
	test-get-pathspec a/aa a/ab/abc >result &&
	echo a/aa >expected &&
	echo a/ab/abc >>expected &&
	test_cmp expected result
'

# a/aa is replaced by a/aa/aaa and a/aa/aab
# reallocation must be done
test_expect_failure '() [a/aa/aaa a/aa/aab a/ab] a/aa a/ab/abc' '
	echo a/aa/aaa >.git/narrow &&
	echo a/aa/aab >>.git/narrow &&
	echo a/ab >>.git/narrow &&
	test-get-pathspec a/aa a/ab/abc >result &&
	echo a/ab/abc >expected &&
	echo a/aa/aaa >>expected &&
	echo a/aa/aab >>expected &&
	test_cmp expected result
'

test_expect_success '() [a b] no pathspec' '
	echo a >.git/narrow &&
	echo b >>.git/narrow &&
	test-get-pathspec >result &&
	echo a >expected &&
	echo b >>expected &&
	test_cmp expected result
'

test_expect_success '(a) no pathspec' '
	: >.git/narrow
	(
	cd a
	test-get-pathspec >result &&
	echo a/ >expected &&
	test_cmp expected result
	)
'

test_expect_success '(a) [a] no pathspec' '
	echo a >.git/narrow &&
	(
	cd a
	test-get-pathspec >result &&
	echo a/ >expected &&
	test_cmp expected result
	)
'

test_expect_success '(a) [a] aa' '
	echo a >.git/narrow &&
	(
	cd a
	test-get-pathspec aa >result &&
	echo a/aa >expected &&
	test_cmp expected result
	)
'

test_expect_failure '(b) [a] no pathspec' '
	echo a >.git/narrow &&
	(
	cd b
	test-get-pathspec >result &&
	echo a >expected &&
	test_cmp expected result
	)
'

test_done
