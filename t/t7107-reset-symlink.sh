#!/bin/sh
#
# Copyright (c) 2014 Charles Bailey
#

test_description='git reset should recover from symlink changes'
. ./test-lib.sh

test_expect_success \
    'creating initial files' \
    'mkdir dir &&
     echo contents >dir/file &&
     git add dir/file &&
     git commit -m initial'

test_expect_success \
    'move to new location and add symlink' \
    'mv dir dir-real &&
     ln -s dir-real dir'

test_expect_success \
    'resetting hard to HEAD' \
    'git reset --hard HEAD'

test_expect_failure \
    'expecting the link to incorrectly remain after reset for now' \
    'test -d dir &&
     test ! -h dir'

test_expect_success \
    'illustrating odd behavior that results in successful reset' \
    'rm dir &&
     git reset --hard HEAD &&
     test -d dir &&
     test ! -h dir &&
     test -f dir/file &&
     test "$(cat dir/file)" = contents &&
     rm -rf dir &&
     ln -s dir-real dir &&
     git reset --hard HEAD'

test_expect_success \
    'checking initial paths at true locations after reset' \
    'test -d dir &&
     test ! -h dir &&
     test -f dir/file &&
     test "$(cat dir/file)" = contents'

test_expect_success \
    'copied path should still exist (effectively untracked)' \
    'test -d dir-real &&
     test ! -h dir-real &&
     test -f dir-real/file &&
     test "$(cat dir-real/file)" = contents'

test_done
