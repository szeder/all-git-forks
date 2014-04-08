#!/bin/sh
#
# Copyright (c) 2014 Diego Lago González
# <diego.lago.gonzalez@gmail.com>
#

test_description='git commit attributes (--attr option)

This script test the commit attributes feature with
command git commit --attr <key=value>.'

. ./test-lib.sh

count=1

test_expect_success 'git commit --attr key=value' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
git commit --attr key=value -m "Commit message."
'

test_expect_success 'git commit -A key=value' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
git commit -A key=value -m "Commit message."
'

test_expect_success 'git commit --attr=key=value' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
git commit --attr=key=value -m "Commit message."
'

test_expect_success 'git commit --attr with utf8 key' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
git commit --attr key→=value -m "Commit message."
'

test_expect_success 'git commit --attr with utf8 value' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
git commit --attr key=valu€ -m "Commit message."
'

test_expect_success 'git commit --attr with a long value' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
git commit --attr "key=Long message for an attribute." -m "Commit message."
'

test_expect_success 'git commit --attr with a very long value' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
git commit --attr "key=Very long message for a commit attribute. This is a very long message to check if commit attributes (extra headers for commit objects) work as expected." -m "Commit message."
'

test_expect_success 'git commit --attr key= (no value)' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
test_must_fail git commit --attr key= -m "Commit message."
'

test_expect_success 'git commit --attr = (no key and no value)' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
test_must_fail git commit --attr = -m "Commit message."
'

test_expect_success 'git commit --attr key (no value and no = sign)' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
test_must_fail git commit --attr key -m "Commit message."
'

test_expect_success 'git commit --attr =value (no key with value)' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
test_must_fail git commit --attr =value -m "Commit message."
'

test_expect_success 'git commit --attr "key =value" (key with space)' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
test_must_fail git commit --attr "key =value" -m "Commit message."
'

test_expect_success 'git commit --attr: key with invalid chars' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
test_must_fail git commit --attr "key
=value" -m "Commit message."
'

test_expect_success 'git commit --attr: value with invalid chars' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
test_must_fail git commit --attr "key=value
with invalid
chars" -m "Commit message."
'

test_expect_success 'git commit --amend: new attribute on amend' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
git commit --amend --attr "new_key=new value" -m "New attribute on amend."
'

test_expect_success 'git commit --amend: replace attribute on amend' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
git commit --amend --attr "new_key=replaced value" -m "Replaced on amend."
'

test_expect_success 'git commit --amend: remove attribute on amend' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
git commit --amend --attr "new_key=" -m "Removed on amend."
'

test_expect_success 'git log -1: get attribute' '
echo test_$count > test &&
count=$(($count+1)) &&
git add test &&
git commit --attr "key=testthis" -m "Commit message." &&
test $(git log -1 --format="%A(key)") = "testthis"
'

test_expect_success 'git log -1: get attribute (key not found)' '
test_must_fail test $(git log -1 --format="%A(key_invalid)") = "testthis"
'

test_expect_success 'git log -1: get attribute (text of key not found)' '
test $(git log -1 --format="%A(key_invalid)") = "%A(key_invalid)"
'

test_expect_success 'git log -1: get attribute (invalid attribute name)' '
test_must_fail test $(git log -1 --format="%A(key)") = "testthis_invalid"
'

test_expect_success 'git log -1: get attribute (key not found but hidden)' '
test "$(git log -1 --format="%A?(key_invalid)")" = ""
'

test_expect_success 'git log -1: get attribute (key with space)' '
test "$(git log -1 --format="% A(key)")" = " testthis"
'

test_expect_success 'git log -1: get attribute (key -hidden- and space)' '
test "$(git log -1 --format="% A?(key)")" = " testthis"
'

test_expect_success 'git log -1: get attribute (key not found but hidden -no space-)' '
test "$(git log -1 --format="% A?(key_invalid)")" = ""
'

test_expect_success 'git commit --amend: check for replaced attribute' '
git commit --amend --attr key=testthat -m "Amend: replace attribute."
test_must_fail test "$(git log -1 --format="%A(key)")" = "testthis"
test "$(git log -1 --format="%A(key)")" = "testthat"
'

test_expect_success 'git commit --amend: check for removed attribute' '
git commit --amend --attr key= -m "Amend: replace attribute."
test_must_fail test "$(git log -1 --format="%A(key)")" = "testthis"
test "$(git log -1 --format="%A(key)")" = "%A(key)"
test "$(git log -1 --format="%A?(key)")" = ""
'

test_done
