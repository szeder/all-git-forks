#!/bin/sh
#
# Copyright (c) 2013 Mathieu Lienard--Mayor et Jorge Garcia
#

test_description='Test git rm error message'

. ./test-lib.sh

cat >expected << EOF
error: the following files have staged content different from both the file and the HEAD:
    foo.txt
    bar.txt
(use -f to force removal)
EOF

test_expect_success 'error message' '
    >foo.txt &&
    >bar.txt &&
    git add foo.txt bar.txt &&
    echo content >foo.txt &&
    echo content >bar.txt &&
    git rm foo.txt bar.txt 2>actual &&
    cat actual &&
    test_cmp expected actual
'

test_done
