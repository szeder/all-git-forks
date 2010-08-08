#!/bin/sh
#
# Copyright (c) 2010 Jon Seymour
#

test_description='git atomic tests

Performs tests on the functions of git atomic
'
. ./test-lib.sh

GIT_AUTHOR_NAME=author@name
GIT_AUTHOR_EMAIL=bogus@email@address
export GIT_AUTHOR_NAME GIT_AUTHOR_EMAIL

test_expect_success \
    'setup' \
    '
    test_commit A &&
    test_commit B &&
    test_commit C &&
    test_commit D &&
    git checkout -b branch A &&
    test_commit X &&
    echo >> B.t &&
    git add B.t &&
    test_commit Y
    true
'

test_expect_success 'no arguments' '
    git atomic
'

test_expect_success 'successful command' \
'
    git atomic true
'

test_expect_success 'unsuccessful command' \
'
    ! git atomic false
'

test_expect_success 'rebase' \
'
     git reset --hard HEAD &&
     git checkout master &&
     MASTER=$(git rev-parse HEAD) &&
     ! git rebase --onto D A Y &&
     git test --conflicted &&
     git rebase --abort &&
     git checkout master &&
     ! git atomic git rebase --onto D A Y &&
     git test --same HEAD refs/heads/master &&
     git test --same HEAD $MASTER
'

test_done
