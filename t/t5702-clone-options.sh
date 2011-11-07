#!/bin/sh

test_description='basic clone options'
. ./test-lib.sh

test_expect_success 'setup' '

	mkdir parent &&
	(cd parent && git init &&
	 echo one >file && git add file &&
	 git commit -m one &&
	 git branch other)

'

test_expect_success 'clone -o' '

	git clone -o foo parent clone-o &&
	(cd clone-o &&
	 git rev-parse --verify refs/remotes/foo/master &&
	 git rev-parse --verify refs/remotes/foo/other)
'

test_expect_success 'redirected clone' '

	git clone "file://$(pwd)/parent" clone-redirected >out 2>err &&
	test ! -s err

'
test_expect_success 'redirected clone -v' '

	git clone --progress "file://$(pwd)/parent" clone-redirected-progress \
		>out 2>err &&
	test -s err

'

test_expect_success 'select one branch to fetch' '
	git clone --progress --fetch=master "file://$(pwd)/parent" clone-select-one &&
	(cd clone-sel &&
	 git rev-parse --verify refs/remotes/origin/master &&
	 test_must_fail git rev-parse --verify refs/remotes/origin/other)
'

test_expect_success 'select several branches to fetch' '
	git clone --progress --fetch=master --fetch=other "file://$(pwd)/parent" clone-select-many &&
	(cd clone-sel2 &&
	 git rev-parse --verify refs/remotes/origin/master &&
	 git rev-parse --verify refs/remotes/origin/other)
'

test_done
