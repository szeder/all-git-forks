#!/bin/sh

test_description='basic narrow repo tests'

. ./test-lib.sh

test_expect_success 'rev-parse --narrow-prefix results empty output on non-narrow repo' '
	git rev-parse --narrow-prefix >actual &&
	test_must_be_empty actual
'

test_expect_success 'empty narrow file rejected' '
	: >.git/narrow &&
	test_must_fail git rev-parse --narrow-prefix
'

test_expect_success 'empty line in narrow file rejected' '
	echo >.git/narrow &&
	test_must_fail git rev-parse --narrow-prefix
'

test_expect_success 'line ending with slash in narrow file rejected' '
	echo abc/ >.git/narrow &&
	test_must_fail git rev-parse --narrow-prefix
'

test_expect_success 'unsorted narrow file rejected' '
	cat >.git/narrow <<-\EOF &&
	def
	abc
	EOF
	test_must_fail git rev-parse --narrow-prefix
'

test_expect_success 'dup lines in narrow file rejected' '
	cat >.git/narrow <<-\EOF &&
	abc
	abc
	EOF
	test_must_fail git rev-parse --narrow-prefix
'

test_expect_success 'good narrow file accepted' '
	cat >.git/narrow <<-\EOF &&
	abc
	def
	EOF
	git rev-parse --narrow-prefix >actual &&
	test_cmp .git/narrow actual
'

test_expect_success 'create narrow index' '
	mkdir a &&
	echo a >.git/narrow &&
	: >a/foo &&
	git add a/foo &&
	test -f .git/index
'

test_expect_success '$GIT_DIR/narrow and index do not match' '
	mkdir b &&
	: >b/foo &&
	echo b >.git/narrow &&
	test_must_fail git add b/foo
'

test_expect_success 'narrow index and normal repo' '
	rm .git/narrow &&
	test_must_fail git add a/foo
'

test_expect_success 'turn to normal index again' '
	rm .git/index &&
	git add a/foo &&
	test_path_is_file .git/index
'

test_expect_success 'normal index and narrow repo' '
	echo a >.git/narrow &&
	test_must_fail git add a/foo
'

test_expect_success 'rev-parse --narrow-base in normal repo' '
	test_must_fail git rev-parse --narrow-base
'

test_expect_success 'update-index --narrow-base in normal repo' '
	EMPTY_TREE=4b825dc642cb6eb9a060e54bf8d69288fbee4904 &&
	test_must_fail git update-index --narrow-base $EMPTY_TREE
'

test_expect_success 'create narrow index' '
	echo t1 >.git/narrow &&
	mkdir t1 t2 t1/t12 &&
	echo 0 >f0 &&
	echo 10 >t1/f10 &&
	echo 120 >t1/t12/f120 &&
	echo 20 >t2/f20 &&
	rm .git/index &&
	git add . &&
	TREE=$(git write-tree) &&
	test "$(git rev-parse --narrow-prefix)" = t1 &&
	git rev-parse --narrow-base >result &&
	echo 0000000000000000000000000000000000000000 >expected &&
	test_cmp expected result &&
	git update-index --narrow-base $TREE &&
	echo $TREE >expected &&
	git rev-parse --narrow-base >result &&
	test_cmp expected result
'

test_done
