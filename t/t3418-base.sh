#!/bin/sh
#
# Copyright (c) 2010 Jon Seymour
#

test_description='git base tests

Checks that git base implements its specification.

'

#
#          G
#         /
# base - A - M - C - D - master - E
#     \     / \
#        B     F
#

. ./test-lib.sh

GIT_AUTHOR_NAME=author@name
GIT_AUTHOR_EMAIL=bogus@email@address
export GIT_AUTHOR_NAME GIT_AUTHOR_EMAIL

ensure_initial_state()
{
    git reset --hard HEAD &&
    git checkout master &&
    git reset --hard D && {
        git update-ref -d refs/bases/master || true
    } && {
        git config --unset branch.master.baseresetcmd || true
    } &&
    ! git base clear
}

test_expect_success 'setup' \
'
    test_commit base &&
    test_commit A &&
    git checkout A^1 &&
    test_commit B &&
    git checkout master &&
    test_merge M B &&
    test_commit C &&
    test_commit D &&
    test_commit E &&
    git checkout M^0 -- &&
    test_commit F &&
    git checkout A^0 -- &&
    test_commit G &&
    git checkout master &&
    git reset --hard D
'

test_expect_success 'empty check when base missing' \
'
	ensure_initial_state &&
	! git base clear &&
	! git base check &&
	! git rev-parse --verify refs/bases/master
'

test_expect_success 'empty check when base hidden (A)' \
'
	ensure_initial_state &&
	! git base set -f A &&
	! git base check &&
	test "$(git base check)" = "" &&
	git rev-parse --verify refs/bases/master &&
	test "$(git rev-parse refs/bases/master)" = "$(git rev-parse A)"
'

test_expect_success 'empty check when base unreachable (E)' \
'
	ensure_initial_state &&
	! git base set -f E &&
	! git base check &&
	test "$(git base check)" = "" &&
	git rev-parse --verify refs/bases/master &&
	test "$(git rev-parse refs/bases/master)" = "$(git rev-parse E)"
'

test_expect_success 'empty check when base unreachable and hidden (G)' \
'
	ensure_initial_state &&
	! git base set -f G &&
	! git base check &&
	test "$(git base check)" = "" &&
	git rev-parse --verify refs/bases/master &&
	test "$(git rev-parse refs/bases/master)" = "$(git rev-parse G)"
'

test_expect_success 'empty check when base reachable and visible (M)' \
'
	ensure_initial_state &&
	git base set -f M &&
	git base check &&
	test "$(git base check)" = "$(git rev-parse M)" &&
	git rev-parse --verify refs/bases/master &&
	test "$(git rev-parse refs/bases/master)" = "$(git rev-parse M)"
'

test_expect_success 'test default commit when clear' \
'
	ensure_initial_state &&
        ! git base clear &&
	git base M &&
	test "$(git base check)" = "$(git rev-parse M)"
'

test_expect_success 'test default commit when set' \
'
	ensure_initial_state &&
        git base set C &&
	git base M &&
	test "$(git base check)" = "$(git rev-parse C)"
'

test_expect_success 'git base - base not set initially' \
'
    ensure_initial_state &&
    ! git base clear &&
    ! git rev-parse --verify refs/bases/master &&
    ! git base &&
    ! git rev-parse --verify refs/bases/master &&
    test -z "$(git base)"
'
test_expect_success 'test default when reference stale' \
'
    ensure_initial_state &&
    ! git base set -f base &&
    ! git base &&
    ! git rev-parse --verify refs/bases/master &&
    test -z "$(git base)"
'

test_expect_success 'test --as-ref does not create or check reference even when empty or stale' \
'
    ! git base clear &&
    test "$(git base --as-ref)" = "refs/bases/master" &&
    ! git base check &&
    ! git base set -f base &&
    ! git base --as-ref &&
    test "$(git rev-parse refs/bases/master)" = "$(git rev-parse base)"
'

test_expect_success 'test set to stale reference ' \
'
    ! git base clear &&
    git base set A &&
    test "$(git base)" = $(git rev-parse M) &&
    git base set B &&
    test $(git base) = $(git rev-parse M) &&
    git base set F &&
    test $(git base) = $(git rev-parse M) &&
    git base set G &&
    test $(git base) = $(git rev-parse M) &&
    git base set E &&
    test $(git base) = $(git rev-parse D)
'

test_expect_success 'test set to good reference ' \
'
    ! git base clear &&
    git base set C &&
    test $(git base) = $(git rev-parse C) &&
    git base set D &&
    test $(git base) = $(git rev-parse D)
'

test_expect_success 'exit codes: ! clear && default' \
'
    ! git base clear &&
    ! git base
'

test_expect_success 'exit codes: ! clear && !check && ! default' \
'
    ! git base clear &&
    ! git base check &&
    ! git base
'

test_expect_success 'exit codes: ! clear && ! set && ! default' \
'
    ! git base clear &&
    ! git base set &&
    ! git base
'

test_expect_success 'test detached head' \
'
    git checkout master^0 &&
    test "$(git base --as-ref)" = "BASE" &&
    git base set base &&
    git base check &&
    git base set M &&
    test "$(git base)" = "$(git rev-parse M)"
'

test_expect_success 'test -b master' \
'
    ! git base -b master clear &&
    git checkout master^0 &&
    test "$(git base -b master --as-ref)" = "refs/bases/master" &&
    git base -b master set base &&
    git base -b master check &&
    git base -b master set M &&
    test "$(git base -b master)" = "$(git rev-parse M)"
'

test_done
