#!/bin/sh

test_description='diff --no-index -L'

. ./test-lib.sh

test_expect_success 'setup' '
	mkdir -p non/git &&
	echo 1 >non/git/a &&
	echo 2 >non/git/b
'

test_expect_success 'git diff --no-index -Lx -Ly' '
  test_expect_code 1 git diff --no-index -Lx -Ly non/git/a non/git/b > actual 
  grep -- "--- x" actual &&
  grep "+++ y" actual
'

test_expect_success 'git diff --no-index -Lx' '
  test_expect_code 1 git diff --no-index -Lx non/git/a non/git/b > actual 
  grep -- "--- x" actual &&
  grep "+++ non/git/b" actual
'

test_done
