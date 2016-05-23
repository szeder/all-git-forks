#!/bin/sh

test_description="Test the svn importer's input handling routines.

These tests provide some simple checks that the line_buffer API
behaves as advertised.

While at it, check that input of newlines and null bytes are handled
correctly.
"
. ./test-lib.sh

test_expect_success 'hello world' '
	echo ">HELLO" >expect &&
	test-line-buffer <<-\EOF >actual &&
	binary 6
	HELLO
	EOF
	test_cmp expect actual
'

test_expect_success '0-length read, send along greeting' '
	echo ">HELLO" >expect &&
	test-line-buffer <<-\EOF >actual &&
	binary 0
	copy 6
	HELLO
	EOF
	test_cmp expect actual
'

test_expect_success !MINGW 'read from file descriptor' '
	rm -f input &&
	echo hello >expect &&
	echo hello >input &&
	echo copy 6 |
	test-line-buffer "&4" 4<input >actual &&
	test_cmp expect actual
'

test_expect_success 'skip, copy null byte' '
	echo Q | q_to_nul >expect &&
	q_to_nul <<-\EOF | test-line-buffer >actual &&
	skip 2
	Q
	copy 2
	Q
	EOF
	test_cmp expect actual
'

test_expect_success 'read null byte' '
	echo ">QhelloQ" | q_to_nul >expect &&
	q_to_nul <<-\EOF | test-line-buffer >actual &&
	binary 8
	QhelloQ
	EOF
	test_cmp expect actual
'

test_expect_success 'long reads are truncated' '
	echo ">foo" >expect &&
	test-line-buffer <<-\EOF >actual &&
	binary 5
	foo
	EOF
	test_cmp expect actual
'

test_expect_success 'long copies are truncated' '
	printf "%s\n" ">" foo >expect &&
	test-line-buffer <<-\EOF >actual &&
	binary 1

	copy 5
	foo
	EOF
	test_cmp expect actual
'

test_expect_success 'long binary reads are truncated' '
	echo ">foo" >expect &&
	test-line-buffer <<-\EOF >actual &&
	binary 5
	foo
	EOF
	test_cmp expect actual
'

test_expect_success 'strbuf_grow and strbuf_release with default flags' '
	test-strbuf grow_release_default
'

test_expect_success 'strbuf_init with not fixed and owned memory ' '
	test-strbuf init_not_fixed
'

test_expect_success 'strbuf_wrap_preallocated multiple tests' '
	test-strbuf preallocated_multiple
'

test_expect_success 'strbuf_grow with wrap_fixed in a basic failure case' '
	test_must_fail test-strbuf grow_fixed_basic_failure
'

test_expect_success 'strbuf_grow with wrap_fixed in a complexe failure case' '
	test_must_fail test-strbuf grow_fixed_complexe_failure
'

test_expect_success 'strbuf_grow with wrap_fixed in a successfull case' '
	test-strbuf grow_fixed_success
'

test_expect_success 'stbuf_detach with wrap_fixed memory' '
	test-strbuf detach_fixed
'

test_expect_success 'stbuf_release with wrap_fixed memory' '
	test-strbuf release_fixed
'

test_done
