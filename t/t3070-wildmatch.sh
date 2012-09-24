#!/bin/sh

test_description='wildmatch tests'

. ./test-lib.sh

test_wildmatch() {
    test_expect_success "wildmatch $*" "
	test-wildmatch $* ../t3070-wildmatch/wildtest.txt >actual &&
	echo 'No wildmatch errors found.' >expected &&
	test_cmp expected actual
    "
}

test_wildmatch -x1
test_wildmatch -x1 -e1
test_wildmatch -x1 -else
test_wildmatch -x2
test_wildmatch -x2 -ese
test_wildmatch -x3
test_wildmatch -x3 -e1
test_wildmatch -x4
test_wildmatch -x4 -e2e
test_wildmatch -x5
test_wildmatch -x5 -es

test_done
