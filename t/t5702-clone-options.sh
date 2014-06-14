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

test_expect_success 'clone creates all tracking branches' '

	git clone parent clone-all &&
	test $(cd clone-all && git branch | wc -l) = 3

'

test_expect_success 'clone with branch only creates one tracking branch' '

	git clone --one-tracking-branch parent clone-single &&
	test $(cd clone-single && git branch | wc -l) = 1

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
