#!/bin/sh
#
# Copyright (c) 2013 Mathieu Lienard--Mayor et Jorge Garcia
#

test_description='Test git rm error message'

. ./test-lib.sh

cat > expect << EOF
error: the following submodules (or one of its nested submodule) use a .git directory:
    foo.txt
(use 'rm -rf' if you really want to remove it including all of its history)
EOF
test_expect_success 'rm file with changes in the index' '
    >foo.txt &&
    git add foo.txt &&
    git commit -m "." &&
    
    
    test_must_fail git rm foo.txt 2>actual &&
    test_cmp expect actual
'

test_done
