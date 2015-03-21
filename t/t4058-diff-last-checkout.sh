#!/bin/sh

test_description='diff can use "-" to refer to the last checkout'

. ./test-lib.sh

test_expect_success 'setup' '
  test_commit 0 file && git branch b0 &&
  test_commit 1 file && git branch b1 &&
  test_commit 2 file && git branch b2
'

test_expect_success '"diff -" does not work initially' '
  test_must_fail git diff -
'

test_expect_success '"diff -" produces diff from last branch' '
  git checkout b1 &&
  test "$(git diff -)" = "$(git diff b2)"
'

test_expect_success '"diff -" produces diff from last detached HEAD' '
  git checkout $(git rev-parse b0) &&
  test "$(git diff -)" = "$(git diff b1)"
'

test_done
