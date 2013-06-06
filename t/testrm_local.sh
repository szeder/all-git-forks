#!/bin/sh
#
# Copyright (c) 2013 Mathieu Lienard--Mayor et Jorge Garcia
#

test_description='Test git rm error message'

. ./test-lib.sh

cat > expect << EOF
error: the following files have local modifications:
    foo.txt
(use --cached to keep the file, or -f to force removal)
EOF
test_expect_success 'rm file with local modification' '
    >foo.txt &&
    git add foo.txt &&
    git commit -m "." &&
    echo content > foo.txt &&
    test_must_fail git rm foo.txt 2>actual &&
    test_cmp expect actual
'

cat > expect << EOF
error: the following files have local modifications:
    bar.txt
EOF
test_expect_success 'rm file with local modification without hints' '
    >bar.txt &&
    git add bar.txt &&
    echo content > bar.txt &&
    git commit -m "." &&
    test_must_fail git -c advice.rmhints=false rm bar.txt 2>actual &&
    test_cmp expect actual
'

test_done
