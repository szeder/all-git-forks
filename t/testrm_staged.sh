#!/bin/sh
#
# Copyright (c) 2013 Mathieu Lienard--Mayor et Jorge Garcia
#

test_description='Test git rm error message'

. ./test-lib.sh

cat > expect << EOF
error: the following files have staged content different from both the file and the HEAD:
    bar.txt
    foo.txt
(use -f to force removal)
EOF
test_expect_success 'rm files with different staged content' '
    >foo.txt &&
    >bar.txt &&
    git add foo.txt bar.txt &&
    echo content >foo.txt &&
    echo content >bar.txt &&
    test_must_fail git rm foo.txt bar.txt 2>actual &&
    test_cmp expect actual
'

cat > expect << EOF
error: the following files have staged content different from both the file and the HEAD:
    bar.txt
    foo.txt
EOF
test_expect_success 'rm files with different staged content without hints' '
    >foo.txt &&
    >bar.txt &&
    git add foo.txt bar.txt &&
    echo content >foo.txt &&
    echo content >bar.txt &&
    test_must_fail git -c advice.rmhints=false rm foo.txt bar.txt 2>actual &&
    test_cmp expect actual
'

test_done
