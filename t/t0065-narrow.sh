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

test_done
