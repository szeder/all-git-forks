#!/bin/sh

test_description='basic clone options'
. ./test-lib.sh

test_expect_success 'setup' '

	mkdir parent &&
	(cd parent && git init &&
	 echo one >file && git add file &&
	 git commit -m one && git branch test1 &&
         git branch test2)

'

test_expect_success 'clone without -a' '

	git clone parent clone-no-a &&
	test $(cd clone-no-a && git branch | wc -l) = 1

'

test_expect_success 'clone -a' '

	git clone -a parent clone-a &&
	test $(cd clone-a && git branch | wc -l) = 3

'

test_expect_success 'clone -o' '

	git clone -o foo parent clone-o &&
	(cd clone-o && git rev-parse --verify refs/remotes/foo/master)

'

test_expect_success 'redirected clone does not show progress' '

	git clone "file://$(pwd)/parent" clone-redirected >out 2>err &&
	! grep % err &&
	test_i18ngrep ! "Checking connectivity" err

'

test_expect_success 'redirected clone -v does show progress' '

	git clone --progress "file://$(pwd)/parent" clone-redirected-progress \
		>out 2>err &&
	grep % err

'

test_done
