#!/bin/sh

test_description='patience diff algorithm'

. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-diff-alternative.sh

# baseline
test_diff_unique ""
test_diff_ignore ""

test_diff_frobnitz "patience"

test_diff_unique "patience"

test_diff_ignore "patience"

test_done
