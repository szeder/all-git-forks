#!/bin/sh
#
# Copyright (c) 2005 Junio C Hamano
#

test_description='git update-index nonsense-path test.

This test creates the following structure in the cache:

    path0       - a file
    path1       - a symlink
    path2/file2 - a file in a directory
    path3/file3 - a file in a directory

and tries to git update-index --add the following:

    path0/file0 - a file in a directory
    path1/file1 - a file in a directory
    path2       - a file
    path3       - a symlink

All of the attempts should fail.
'

. ./test-lib.sh

test_expect_success 'git update-index --add to add various paths' '

	mkdir path2 path3 &&
	date >path0 &&
	test_ln_s_add xyzzy path1 &&
	date >path2/file2 &&
	date >path3/file3 &&
	test_when_finished "rm -fr path0 path1 path2 path3" &&
	git update-index --add -- path0 path1 path2/file2 path3/file3
'

test_expect_success 'git update-index to add conflicting path path0/file0 should fail' '

	mkdir path0 &&
	date >path0/file0 &&
	test_must_fail git update-index --add -- path0/file0
'

test_expect_success 'git update-index to add conflicting path path1/file1 should fail' '

	mkdir path1 &&
	date >path1/file1 &&
	test_must_fail git update-index --add -- path1/file1
'

test_expect_success 'git update-index to add conflicting file path2 should fail' '

	date >path2 &&
	test_must_fail git update-index --add -- path2
'

test_expect_success 'git update-index to add conflicting symlink path3 should fail' '

	test_ln_s xyzzy path3 &&
	test_must_fail git update-index --add -- path3
'

test_done
