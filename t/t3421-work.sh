#!/bin/sh
#
# Copyright (c) 2010 Jon Seymour
#

test_description='git work tests

Performs tests on the functions of git work
'
#
#       H-I
#      /
# A-B-C---M-P-Q
#  \     /
#   X-Y-Z
#

. ./test-lib.sh
. $(git --exec-path)/git-test-lib

GIT_AUTHOR_NAME=author@name
GIT_AUTHOR_EMAIL=bogus@email@address
export GIT_AUTHOR_NAME GIT_AUTHOR_EMAIL

reset_test()
{
	git reset --hard HEAD &&
	git checkout master &&
	git base set M && {
	test_condition -q --not-branch-exists test ||
	git branch -D test
	} &&
	test_condition -q --not-branch-exists topic || {
	git base init -d -b topic &&
	git branch -D topic
	}
}

test_expect_success 'setup' \
	'
	test_commit A &&
	test_commit B &&
	test_commit C &&
	git checkout -b fork-X A &&
	test_commit X &&
	test_commit Y &&
	test_commit Z &&
	git checkout master &&
	test_merge M Z &&
	git base -q set HEAD &&
	test_commit P &&
	test_commit Q &&
	git checkout -b fork-H C &&
	test_commit H &&
	test_commit I &&
	git checkout master &&
	true
'

test_expect_success 'work --as-refs' \
'
	test "$(git work --as-refs)" = "refs/bases/master..master"
'

test_expect_success 'work' \
'
	test "$(git work)" = "$(git rev-parse M)..$(git rev-parse Q)"
'

test_expect_success 'work list' \
'
	test "$(git rev-parse Q; git rev-parse P)" = "$(git work list)"
'

test_expect_success 'git work rebase Z <=> rebase --onto Z M..Q' \
'
	reset_test &&
	git checkout -b test Q &&
	git base -q set M &&
	git work rebase Z &&
	test_condition --same $(git base) Z &&
	test_condition --checked-out test &&
	test "$(git diff M Q | git patch-id)" = "$(git diff $(git work) | git patch-id)"
'

test_expect_success 'git work merge I <=> rebase --onto I M..Q' \
'
	reset_test &&
	git checkout -b test Q &&
	git base -q set M &&
	git work merge I &&
	test_condition --not-same I $(git base) &&
	test_condition --checked-out test &&
	test "$(git diff M Q | git patch-id)" = "$(git diff $(git work) | git patch-id)"
'

test_expect_success 'git work create topic I from P..Q' \
'
	reset_test &&
	git checkout -b test Q &&
	git base -q set M &&
	git work create topic P I &&
	test_condition --same $(git base -b topic) I &&
	test_condition --checked-out test &&
	test "$(git diff I topic | git patch-id)" = "$(git diff P Q | git patch-id)" &&
	test "$(git diff $(git base) test | git patch-id)" = "$(git diff M P | git patch-id)"
'

test_expect_success 'git work create topic --pivot-first I from P..Q' \
'
	reset_test &&
	git checkout -b test Q &&
	git base -q set M &&
	git work create --pivot-first topic P I &&
	test_condition  --same $(git base -b topic) I &&
	test_condition  --checked-out test &&
	test "$(git diff I topic | git patch-id)" = "$(git diff M P | git patch-id)" &&
	test "$(git diff $(git base)  test | git patch-id)" = "$(git diff P Q | git patch-id)"
'

test_expect_success 'git work update topic I from P..Q' \
'
	reset_test &&
	git checkout -b test Q &&
	git base -q set M &&
	git branch topic I &&
	git work update topic P &&
	test_condition  --same "$(git base -b topic)" I &&
	test_condition  --checked-out test &&
	test "$(git diff I topic | git patch-id)" = "$(git diff P Q | git patch-id)" &&
	test "$(git diff $(git base)  test | git patch-id)" = "$(git diff M P | git patch-id)"
'

test_expect_success 'git work update --pivot-first topic I from P..Q' \
'
	reset_test &&
	git checkout -b test Q &&
	git base -q set M &&
	git branch topic I &&
	git work update --pivot-first topic P &&
	test_condition  --same "$(git base -b topic)" I &&
	test_condition  --checked-out test &&
	test "$(git diff I topic | git patch-id)" = "$(git diff M P | git patch-id)" &&
	test "$(git diff $(git base)  test | git patch-id)" = "$(git diff P Q | git patch-id)"
'

test_expect_success 'git work update topic I from P..Q - unitialized topic' \
'
	git base init -d topic &&
	reset_test &&
	git checkout -b test Q &&
	git base -q set M &&
	git branch topic I &&
	git work update topic P &&
	test_condition  --same "$(git base -b topic)" I &&
	test_condition  --checked-out test &&
	test "$(git diff I topic | git patch-id)" = "$(git diff P Q | git patch-id)" &&
	test "$(git diff $(git base)  test | git patch-id)" = "$(git diff M P | git patch-id)"
'

test_expect_success 'git work pivot P' \
'
	reset_test &&
	git checkout -b test Q &&
	git base -q set M &&
	git work pivot P &&
	test_condition  --same "$(git base -b test)" M &&
	test_condition  --checked-out test &&
	test "$(git diff M test | git patch-id)" = "$(git diff M Q | git patch-id)" &&
	test "$(git diff M test~1 | git patch-id)" = "$(git diff P Q | git patch-id)" &&
	test "$(git diff test~1 test | git patch-id)" = "$(git diff M P | git patch-id)"
'

test_done
