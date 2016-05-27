#!/bin/sh

test_description="Test the strbuf API.

These tests provide some simple checks to certify that the strbuf API has the good behaviour.
"
. ./test-lib.sh

test_expect_success 'strbuf_grow and strbuf_release with default flags' '
	test-strbuf grow_release_default
'

test_expect_success 'strbuf_init with not fixed and owned memory ' '
	test-strbuf init_not_fixed
'

test_expect_success 'default grow and release' '
	test-strbuf release_free
'

test_expect_success 'overflow while calling strbuf_grow' '
	test_must_fail test-strbuf grow_overflow
'

test_expect_success 'strbuf_wrap_preallocated multiple tests' '
	test-strbuf preallocated_multiple
'

test_expect_success 'strbuf_wrap_preallocated NULL initialization' '
	test_must_fail test-strbuf preallocated_NULL
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