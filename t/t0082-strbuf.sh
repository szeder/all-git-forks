#!/bin/sh

test_description="Test the strbuf API.
"
. ./test-lib.sh

test_expect_success 'basic test on strbuf_grow()' '
	test-strbuf basic_grow
'

test_expect_success 'check strbuf behavior in usual use cases ' '
	test-strbuf strbuf_check_behavior
'

test_expect_success 'overflow while calling strbuf_grow' '
	test_must_fail test-strbuf grow_overflow
'

test_expect_success 'check preallocated strbuf behavior in usual use cases' '
	test-strbuf preallocated_check_behavior
'

test_expect_success 'strbuf_wrap_preallocated NULL initialization' '
	test_must_fail test-strbuf preallocated_NULL
'

test_expect_success 'strbuf_grow with wrap_fixed overflow' '
	test_must_fail test-strbuf grow_fixed_overflow
'

test_expect_success 'strbuf_grow with wrap_fixed minimum overflow' '
	test_must_fail test-strbuf grow_fixed_overflow_min
'

test_expect_success 'strbuf_grow with wrap_fixed in a successful case' '
	test-strbuf grow_fixed_success
'

test_expect_success 'stbuf_detach with wrap_fixed memory' '
	test-strbuf detach_fixed
'

test_expect_success 'stbuf_release with wrap_fixed memory' '
	test-strbuf release_fixed
'

test_done
