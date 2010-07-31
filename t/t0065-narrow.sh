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

test_done
