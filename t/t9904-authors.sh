#!/bin/sh

test_description='test authors'

. ./test-lib.sh

check_split() {
	echo "$1 -> $2" >expect
	test_expect_success "split '$1'" "
	test-authors split '$1' >actual &&
	test_cmp expect actual
	"
}

check_split 'xxx' 'error'
check_split 'Some Guy <sg@example.com> 1484387401 +0100' 'Some Guy <sg@example.com>'
check_split 'Some Guy <sg@example.com>,Another Pal <ap@example.com> 1484387401 +0100' 'Some Guy <sg@example.com>,Another Pal <ap@example.com>'
check_split 'Some Guy <sg@example.com>,Another Pal <ap@example.com>' 'Some Guy <sg@example.com>,Another Pal <ap@example.com>'

check_has_multiple() {
	echo "$1 -> $2" >expect
	test_expect_success "has multiple authors '$1'" "
	test-authors has-multiple '$1' >actual &&
	test_cmp expect actual
	"
}

check_has_multiple 'abc' 'no'
check_has_multiple 'Some Guy <sg@example.com>' 'no'
check_has_multiple 'Some Guy <sg@example.com>,Another Pal <ap@example.com>' 'yes'

test_done
