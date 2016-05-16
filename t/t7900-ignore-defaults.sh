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
	git ignore ignoreme.txt &&
	echo "ignoreme.txt"  >expect &&
	cat .gitignore >actual &&
	test_cmp expect actual
'

test_expect_success 'ignore in subdir' '
    rm .gitignore &&
    mkdir -p "sub/dir with space" &&
    echo a >"sub/dir with space/ignoreme.txt" &&
    (
        cd "sub/dir with space" &&
        git ignore ignoreme.txt
    ) &&
	echo "sub/dir with space/ignoreme.txt"  >expect &&
	cat .gitignore >actual &&
	test_cmp expect actual
'

test_expect_success 'ignore extentions at root' '
    rm .gitignore &&
    echo a >ignoreme.txt &&
    git ignore -e ignoreme.txt &&
    echo "*.txt" >expect &&
    cat .gitignore >actual &&
    test_cmp expect actual
'

test_expect_success 'ignore extentions in subdir' '
    rm .gitignore &&
    mkdir -p "sub/dir with space" &&
    echo a >"sub/dir with space/ignoreme.txt" &&
    (
      cd "sub/dir with space" &&
      git ignore -e ignoreme.txt
    ) &&
    echo "sub/dir with space/*.txt" >expect &&
    cat .gitignore >actual &&
    test_cmp expect actual
'

test_expect_success 'ignore extentions anywhere' '
    rm .gitignore &&
    mkdir -p "sub/dir with space" &&
    echo a >"sub/dir with space/ignoreme.txt" &&
    (
      cd "sub/dir with space" &&
      git ignore -E ignoreme.txt
    ) &&
    echo "**/*.txt" >expect &&
    cat .gitignore >actual &&
    test_cmp expect actual
'

test_expect_success 'ignore directory' '
    rm .gitignore &&
    mkdir -p "sub/dir with space" &&
    echo a >"sub/dir with space/ignoreme.txt" &&
    (
      cd "sub/dir with space" &&
      git ignore -d ignoreme.txt
    ) &&
    echo "sub/dir with space/*" >expect &&
    cat .gitignore >actual &&
    test_cmp expect actual
'

test_expect_success 'ignore filename anywhere' '
    rm .gitignore &&
    mkdir -p "sub/dir with space" &&
    echo a >"sub/dir with space/ignoreme.txt" &&
    (
      cd "sub/dir with space" &&
      git ignore -a ignoreme.txt
    ) &&
    echo "**/ignoreme.txt" >expect &&
    cat .gitignore >actual &&
    test_cmp expect actual
'

test_expect_success 'dry run does not write anything' '
    rm .gitignore &&
    echo a >ignoreme.txt &&
    git ignore -n ignoreme.txt >output &&
    grep "^DRY-RUN!" <output &&
    test_path_is_missing .gitignore
'

test_expect_success 'parent-level set to current dir' '
    mkdir -p "sub/dir with space" &&
    echo a >"sub/dir with space/ignoreme.txt" &&
    (
      cd "sub/dir with space" &&
      git ignore -p 0 ignoreme.txt
    ) &&
    echo "ignoreme.txt" >expect &&
    cat "sub/dir with space/.gitignore" >actual &&
    test_cmp expect actual
'

test_expect_success 'parent-level set to dir outside of repo top-level' '
    mkdir -p "sub/dir with space" &&
    echo a >"sub/dir with space/ignoreme.txt" &&
    (
      cd "sub/dir with space" &&
      git ignore -p 2 ignoreme.txt >output
    ) &&
    grep "^WARNING" <"sub/dir with space/output" &&
    echo "sub/dir with space/ignoreme.txt" >expect &&
    cat .gitignore >actual &&
    test_cmp expect actual
'

test_expect_success 'parent-level set to mutliple gitignores' '
    mkdir -p "sub/dir1 with space/test" &&
    echo a >"sub/dir1 with space/test/ignoreme.txt" &&
    mkdir -p "sub/dir2 with space/test" &&
    echo a >"sub/dir2 with space/test/ignoreme.txt" &&
    git ignore -p 1 "sub/dir1 with space/test/ignoreme.txt" "sub/dir2 with space/test/ignoreme.txt" &&
    echo "test/ignoreme.txt" >expect &&
    cat "sub/dir1 with space/.gitignore" >actual &&
    test_cmp expect actual &&
    cat "sub/dir2 with space/.gitignore" >actual &&
    test_cmp expect actual
'

test_done
