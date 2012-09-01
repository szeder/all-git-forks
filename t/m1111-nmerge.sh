#!/bin/sh
#
# Copyright (c) 2012 Clemens Buchacher
#

test_description='New merge tests'

. ./test-lib.sh

test_expect_success \
	'setup' \
	"test_commit a &&
	 test_commit b &&
	 test_commit c"

cat >expected <<EOF
a.t
b.t
c.t
EOF

test_expect_success \
	'reset a, nmerge c' \
	"git reset a &&
	 git nmerge c &&
	 git ls-files -c >actual &&
	 test_cmp expected actual"

test_done
