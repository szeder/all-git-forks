#!/bin/sh
#
# Copyright (c) 2008 Christian Couder
#
test_description='Test git bisect replace functionality'

exec </dev/null

. ./test-lib.sh

add_and_commit_file()
{
    _file="$1"
    _msg="$2"

    git add $_file || return $?
    test_tick || return $?
    git commit --quiet -m "$_file: $_msg"
}

HASH1=
HASH2=
HASH3=
HASH4=
HASH5=
HASH6=
HASH7=

test_expect_success 'set up buggy branch' '
     echo "line 1" >> hello &&
     echo "line 2" >> hello &&
     echo "line 3" >> hello &&
     echo "line 4" >> hello &&
     add_and_commit_file hello "4 lines" &&
     HASH1=$(git rev-parse --verify HEAD) &&
     echo "line BUG" >> hello &&
     echo "line 6" >> hello &&
     echo "line 7" >> hello &&
     echo "line 8" >> hello &&
     add_and_commit_file hello "4 more lines with a BUG" &&
     HASH2=$(git rev-parse --verify HEAD) &&
     echo "line 9" >> hello &&
     echo "line 10" >> hello &&
     add_and_commit_file hello "2 more lines" &&
     HASH3=$(git rev-parse --verify HEAD) &&
     echo "line 11" >> hello &&
     add_and_commit_file hello "1 more line" &&
     HASH4=$(git rev-parse --verify HEAD) &&
     sed -e "s/BUG/5/" hello > hello.new &&
     mv hello.new hello &&
     add_and_commit_file hello "BUG fixed" &&
     HASH5=$(git rev-parse --verify HEAD) &&
     echo "line 12" >> hello &&
     echo "line 13" >> hello &&
     add_and_commit_file hello "2 more lines" &&
     HASH6=$(git rev-parse --verify HEAD)
     echo "line 14" >> hello &&
     echo "line 15" >> hello &&
     echo "line 16" >> hello &&
     add_and_commit_file hello "again 3 more lines" &&
     HASH7=$(git rev-parse --verify HEAD)
'

HASHFIX2=
HASHFIX3=
HASHFIX4=

test_expect_success 'set up fixed branch' '
     git checkout $HASH1 &&
     echo "line 5" >> hello &&
     echo "line 6" >> hello &&
     echo "line 7" >> hello &&
     echo "line 8" >> hello &&
     add_and_commit_file hello "4 more lines with no BUG" &&
     HASHFIX2=$(git rev-parse --verify HEAD) &&
     git cherry-pick $HASH3 &&
     HASHFIX3=$(git rev-parse --verify HEAD) &&
     git cherry-pick $HASH4 &&
     HASHFIX4=$(git rev-parse --verify HEAD)
'

test_expect_success '"git bisect replace" buggy branch with fixed one' '
     git bisect replace $HASH5 HEAD
'

test_expect_success 'replace works when bisecting with a later bad commit' '
     git rev-list --bisect-all $HASH7 > rev_list.txt &&
     grep $HASHFIX2 rev_list.txt &&
     grep $HASHFIX3 rev_list.txt &&
     grep $HASHFIX4 rev_list.txt &&
     test_must_fail grep $HASH2 rev_list.txt &&
     test_must_fail grep $HASH3 rev_list.txt &&
     test_must_fail grep $HASH4 rev_list.txt
'

test_expect_success 'replace works starting just after replaced commit' '
     git rev-list --bisect-all $HASH6 > rev_list.txt &&
     grep $HASHFIX2 rev_list.txt &&
     grep $HASHFIX3 rev_list.txt &&
     grep $HASHFIX4 rev_list.txt &&
     test_must_fail grep $HASH2 rev_list.txt &&
     test_must_fail grep $HASH3 rev_list.txt &&
     test_must_fail grep $HASH4 rev_list.txt
'

test_expect_success 'replace works starting from replaced commit' '
     git rev-list --bisect-all $HASH5 > rev_list.txt &&
     grep $HASHFIX2 rev_list.txt &&
     grep $HASHFIX3 rev_list.txt &&
     grep $HASHFIX4 rev_list.txt &&
     test_must_fail grep $HASH2 rev_list.txt &&
     test_must_fail grep $HASH3 rev_list.txt &&
     test_must_fail grep $HASH4 rev_list.txt
'

test_expect_success 'standard bisect works' '
     git bisect start $HASH6 $HASH1 &&
     test "$(git rev-parse --verify HEAD)" = "$HASHFIX3" &&
     git bisect good &&
     test "$(git rev-parse --verify HEAD)" = "$HASH5" &&
     git bisect bad &&
     test "$(git rev-parse --verify HEAD)" = "$HASHFIX4" &&
     git bisect bad > my_bisect_log.txt &&
     grep "$HASHFIX4 is first bad commit" my_bisect_log.txt &&
     git bisect reset
'

test_expect_success '"git rev-list --bisect-replace" works' '
     echo "$HASH7" >> rev_list.expect &&
     echo "$HASH6" >> rev_list.expect &&
     echo "$HASH5" >> rev_list.expect &&
     echo "$HASHFIX4" >> rev_list.expect &&
     echo "$HASHFIX3" >> rev_list.expect &&
     echo "$HASHFIX2" >> rev_list.expect &&
     echo "$HASH1" >> rev_list.expect &&
     git rev-list --bisect-replace $HASH7 > rev_list.output &&
     test_cmp rev_list.expect rev_list.output
'

#
#
test_done
