#!/bin/sh
#
# Copyright (c) 2013 Mathieu Lienard--Mayor et Jorge Garcia
#

test_description='Test git rm error message'

. ./test-lib.sh

cat > expect << EOF
error: the following files have changes staged in the index:
    foo.txt
(use --cached to keep the file, or -f to force removal)
EOF
test_expect_success 'rm file with changes in the index' '
    >foo.txt &&
    git add foo.txt &&
    test_must_fail git rm foo.txt 2>actual &&
    test_cmp expect actual
'

cat > expect << EOF
error: the following files have changes staged in the index:
    foo.txt
EOF
test_expect_success 'rm file with changes in the index without hints' '
    >foo.txt &&
    git add foo.txt  &&
    test_must_fail git -c advice.rmhints=false rm foo.txt 2>actual &&
    test_cmp expect actual
'

test_done
