#!/bin/sh
#
# Copyright (c) 2016 Thurston Stone
#


test_description='check-git-ignore-cmd

This will test the default behaviors of the git ignore command.
'

. ./test-lib.sh

test_expect_success 'setup' '
    echo a >a &&
    git add a &&
    git commit -m"adding initial files"
'

test_expect_success 'ignore at root' '
    echo a >ignoreme.txt &&
    test_pause &&
	git ignore ignoreme.txt &&
	echo "ignoreme.txt"  >expect &&
	cat .gitignore >actual &&
	test_cmp expect actual
'

test_done
